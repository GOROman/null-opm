# Clean-room policy

null-opm is a clean-room implementation of the YAMAHA YM2151 (OPM).

## The rule

Contributors **must not read, copy, translate or paraphrase** the source code of
any existing YM2151 emulator or any emulator of a related Yamaha FM chip. This
includes, but is not limited to: Nuked-OPM, ymfm, MAME (`ym2151.cpp`, `ymfm_*`),
fmgen, X68Sound, the VGMPlay chip cores, and any derivative of them.

Pull requests affirm this in the PR template.

## Allowed sources

- YAMAHA YM2151 datasheet and the YM2151 application manual (register map,
  timing diagrams, rate/time tables, DT1/DT2 tables, LFO/PMS/AMS tables).
- YAMAHA YM3012 (DAC) datasheet (floating-point output format).
- General FM-synthesis theory (log-sin / exponential table structure, phase
  accumulators, envelope generators).
- Public die-analysis write-ups and discussion that describe *behaviour* of the
  hardware in prose or diagrams.
- Measurements of real hardware.

## Black-box comparison

Comparing null-opm's **audio output** with the output of other emulators is
allowed and encouraged. Reference WAV files are generated with those
emulators' own tools; their source is never linked, included or read. See
`docs/TESTING.md`.

## Derivation log

Every non-obvious constant in the core carries a comment naming the document
and table it was derived from. Values that were obtained by *fitting against
reference output* (not from a document) are marked `TUNED:` so that their
provenance is visible.
