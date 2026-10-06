## Summary

## Clean-room affirmation

- [ ] I did not read, copy, translate or paraphrase the source code of any YM2151 / OPM / OPN / OPL emulator while writing this change (see `docs/CLEANROOM.md`).
- [ ] Every new non-obvious constant names its source document, or is marked `TUNED:`.

## Verification

- [ ] `ctest` passes
- [ ] Golden renders unchanged, or regenerated with `NULLOPM_UPDATE_GOLDEN=1` and the reason is explained above
