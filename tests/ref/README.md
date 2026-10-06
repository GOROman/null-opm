# Reference recordings

This directory holds reference WAV files used by `tests/scripts/compare_all.py`.
They are **not committed**: `manifest.json` lists each entry with a URL and a
SHA-256, and `fetch_refs.py` downloads and verifies them.

Entries come from two kinds of sources:

- **Other emulators** (black-box): the WAV is produced with that emulator's own
  command line player from the listed input. Nothing from its source code is
  used; see `docs/CLEANROOM.md`.
- **Real hardware**: a YM2151 + YM3012 board captured through an ADC. These are
  compared with `--align` and `--normalize` because the analog path adds gain,
  offset and filtering.

Manifest entry fields:

```json
{
  "name": "x68k_song1_nuked",
  "input": "tests/golden/inputs/x68k_song1.vgm",
  "url": "https://example.invalid/x68k_song1_nuked.wav",
  "sha256": "…",
  "source": "emulator: <name/version>  |  hardware: <board, ADC>",
  "clock": 4000000,
  "kind": "emulator|hardware",
  "notes": ""
}
```
