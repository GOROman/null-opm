# Testing

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```

## Unit tests (`tests/test_*.cpp`, doctest)

Expected values are derived from datasheet formulas and written as explicit
constants with the formula in a comment. They cover tables, phase generator,
envelope timing, algorithms/feedback/DAC, LFO, noise, timers and the bus.

## Golden renders (`tests/golden/`)

Short register logs rendered by null-opm itself; the WAVs are committed and
compared bit-exactly. They catch unintended changes, not inaccuracy. After an
intentional change:

```sh
NULLOPM_UPDATE_GOLDEN=1 ./build/tests/nullopm_tests -tc="golden*"
```

## Reference comparison (`tests/ref/`)

1. Add an entry to `tests/ref/manifest.json` (see `tests/ref/README.md`).
2. `python3 -I tests/scripts/fetch_refs.py`
3. `python3 -I tests/scripts/compare_all.py build --markdown`

`wavdiff` reports sample-exact ratio, max abs error, RMS and SNR. For emulator
references the goal is ≥ 99.9 % sample-exact with max abs ≤ 1 elsewhere; for
hardware captures use `--align` and `--normalize` and judge by SNR.

## Tuning loop

Constants marked `TUNED:` in `src/opm.cpp` (slot order, LFO divider, PMS scale,
DAC rounding) are the ones expected to move while matching reference output.
