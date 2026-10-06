# Register map

Writes go through the two-port bus: A0 = 0 selects the address, A0 = 1 writes
data. Each data write is latched and committed at the next internal cycle; the
busy flag (status bit 7) stays set for `kBusyCycles`. Writing while busy
replaces the pending write (the earlier one is lost, counted by
`dropped_writes()`).

| Addr | Bits | Field | Notes |
|---|---|---|---|
| 0x01 | 1 | LFO reset | While 1 the LFO phase is held at 0 |
| 0x08 | 6..3, 2..0 | KON C2 C1 M2 M1, channel | Key on/off per operator |
| 0x0F | 7, 4..0 | NE, NFRQ | Noise on channel 7 C2 |
| 0x10 | 7..0 | Timer A bits 9..2 | |
| 0x11 | 1..0 | Timer A bits 1..0 | |
| 0x12 | 7..0 | Timer B | |
| 0x14 | 7, 5, 4, 3, 2, 1, 0 | CSM, F-reset B, F-reset A, IRQ-en B, IRQ-en A, load B, load A | Reset bits act on write |
| 0x18 | 7..0 | LFRQ | High nibble exponent, low nibble mantissa |
| 0x19 | 7, 6..0 | 1 = PMD / 0 = AMD, depth | |
| 0x1B | 7, 6, 1..0 | CT2, CT1, LFO waveform | 0 saw, 1 square, 2 triangle, 3 noise |
| 0x20+ch | 7, 6, 5..3, 2..0 | R, L, FB, CON | |
| 0x28+ch | 6..4, 3..0 | Octave, note code | Codes 3/7/11/15 = next code |
| 0x30+ch | 7..2 | KF | 1/64 semitone |
| 0x38+ch | 6..4, 1..0 | PMS, AMS | |
| 0x40+op | 6..4, 3..0 | DT1, MUL | op = slot*8 + ch, slot: 0 M1, 1 M2, 2 C1, 3 C2 |
| 0x60+op | 6..0 | TL | 0.75 dB per step |
| 0x80+op | 7..6, 4..0 | KS, AR | |
| 0xA0+op | 7, 4..0 | AMS-EN, D1R | |
| 0xC0+op | 7..6, 3..0 | DT2, D2R | DT2: 0 / 600 / 781 / 950 cents |
| 0xE0+op | 7..4, 3..0 | D1L, RR | |

Status register (read): bit 7 busy, bit 1 Timer B flag, bit 0 Timer A flag.
