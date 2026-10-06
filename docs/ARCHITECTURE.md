# Architecture

## Timing model

| Quantity | Value |
|---|---|
| Master clock φM | 3.579545 MHz (configurable) |
| Internal clock φ1 | φM / 2 |
| Operator slots per sample | 32 (8 channels × 4 operators) |
| φ1 cycles per sample | 32 (`kCyclesPerSample`), one per slot |
| Output rate | φM / 64 = 55930 Hz |

`Opm::clock()` advances one φ1 cycle. `generate()` is a loop of 32 `clock()` calls
per sample. Inside `clock()`:

1. A pending bus write (one-deep latch) is committed to the register file. The
   busy flag counts down `kBusyCycles` (34 φ1 cycles ≈ 68 φM, the datasheet's
   minimum write interval).
2. At cycle 0 of a sample: timers, EG global counter and LFO are updated
   (`begin_sample()`).
3. Every 16 cycles (φM / 32): the noise LFSR prescaler ticks.
4. The operator for this slot is processed (`run_slot()`).
5. At cycle 31: channels are panned, mixed and passed through the DAC model
   (`end_sample()`).

Operator index = `slot * 8 + ch` where slot 0..3 = M1, M2, C1, C2, matching the
register map (`0x40 + slot*8 + ch`). Slots are processed in that order. A
modulator processed earlier in the sample feeds its carrier in the same sample;
one processed later feeds the next sample (`slot_to_op()` is the place to change
this; see `TUNED` notes).

## Phase generator

```
pitch index (1/64 semitone) = octave*768 + semitone*64 + KF + DT2[dt2] + PM(ch)
inc20 = (kFnum[index % 768] << (index / 768)) >> 8
inc20 = inc20 ± kDt1[dt1][KC >> 2]
inc20 = mul ? inc20 * mul : inc20 >> 1
phase (20 bit) += inc20
```

`kFnum` is 768 entries of `2^(i/768) * base`, base = C#0 at 3.58 MHz with 8
fractional bits. KC note codes 3, 7, 11, 15 duplicate the next code. The DT1
LSB is `fs / 2^20 = 0.0533 Hz`, which is how the datasheet's DT1 Hz table was
converted to integers.

## Operator

```
idx10 = (phase >> 10) + modulation      (M1: (out0 + out1) >> (10 - FB); others: sum of inputs >> 1)
att   = min(1023, eg_level + TL<<3 + AM)
value = exp((att << 2) + logsin(idx10))  -> 14-bit signed
```

`kLogSin[256]` = `round(-log2(sin((i+0.5)/256 * π/2)) * 256)` (quarter wave,
mirrored/negated by idx bits 8 and 9). `kExp[256]` = `round((2^(i/256) - 1) * 1024)`,
looked up with the inverted low 8 bits, bit 10 implied, shifted right by the
integer part.

## Envelope generator

10-bit attenuation, states attack → decay1 → decay2 (sustain) → release.
Effective rate `r = min(63, 2*R + (KC>>2) >> (3-KS))`, `R = 0` means frozen.
For `r < 48` a step happens every `2^(11 - r/4)` samples with `4 + (r&3)` of
every 8 steps incrementing by 1; for `r >= 48` every sample increments by the
1/2/4/8 pattern rows. Attack: `level += (-(level+1) * inc) >> 4`. D1L 15 maps
to level 992 (−93 dB). RR is used as `2*RR + 1`.

## LFO

Counter += `(16 + LFRQ[3:0]) << LFRQ[7:4]` per sample; the 8-bit waveform phase
steps each `kLfoStep` (TUNED to 32.6 Hz at LFRQ 0xFF). Waveforms: saw, square,
triangle, noise (17-bit LFSR). PM: `±127 * PMD >> 7`, then `kPmsScale[PMS] >> 6`
into 1/64-semitone units (datasheet max deviations 5…700 cents). AM:
`0..255 * AMD >> 8`, then AMS 1/2/3 = `>>3`, `>>1`, `>>0` (1.4 / 5.9 / 11.8 dB).

## Noise

17-bit LFSR (`x^17 + x^14 + 1`) clocked at φM / (32 × (32 − NFRQ)). With NE set,
channel 7's C2 outputs ±full-scale with the LFSR bit as sign, still shaped by EG,
TL and AM.

## Timers

Timer A: 10 bits, +1 per sample, period `1024 − N`. Timer B: 8 bits, +1 per 16
samples, period `16 × (256 − N)`. Overflow reloads; if the IRQ enable bit is set
the status flag is raised and the IRQ pin follows `status != 0`. CSM keys every
operator for one sample on Timer A overflow.

## Output

Carriers of a channel are summed, routed by the L/R bits, 8 channels are mixed,
clamped to 16 bits, then quantized the way the YM3012 receives them: a 3-bit
exponent and a 10-bit mantissa, so values beyond ±511 lose their low bits
(`detail::dac_quantize`).

## State

Everything lives in one POD `detail::State` (about 2 KB) inside `Opm`. No heap,
no globals, no floating point; copying an `Opm` is a save state.
