# null-opm

Clean-room, cycle-accurate YAMAHA **YM2151 (OPM)** FM synthesizer emulator in C++17.

- Integer only, allocation-free, no exceptions / RTTI in the core
- Multiple independent chip instances (`nullopm::Opm` is a plain copyable object)
- Targets: desktop (macOS / Linux / Windows), WebAssembly (Emscripten), MCUs (ESP32, RP2040)
- Native output at `clock / 64` (55930 Hz at 3.579545 MHz)
- MIT licensed. See [docs/CLEANROOM.md](docs/CLEANROOM.md) for the no-copy policy.

## Status

| Feature | State |
|---|---|
| Register file, write latch, busy flag | done (M0) |
| Phase generator (KC/KF/DT1/DT2/MUL), log-sin/exp | done (M1) |
| Envelope generator | done (M2) |
| Algorithms, feedback, pan, YM3012 DAC | done (M3) |
| LFO (PM/AM) | done (M4) |
| Noise | planned (M5) |
| Timers, IRQ, CT, CSM | planned (M6) |
| VGM → WAV tool | done (M7, `vgm2wav`) |
| Reference comparison harness | planned (M8) |
| WASM demo / MCU examples | planned (M9/M10) |

## Build

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

WebAssembly:

```sh
emcmake cmake --preset wasm && cmake --build --preset wasm
```

## Tools

```sh
./build/tools/vgm2wav song.vgm song.wav              # native 55930 Hz
./build/tools/vgm2wav --rate 44100 --loops 2 song.vgm song.wav
./build/tools/vgm2wav song.vgm song.wav --regdump song.txt   # text register log
```

Text register logs (`.txt`) are also accepted as input: `clock <hz>`, `w <addr> <data>`,
`wait <n/44100 s>`, `t<time> <addr> <data>`, `end <time>`. `.vgz` needs zlib at build time.

## Use

```cpp
#include "nullopm/opm.hpp"
nullopm::Opm chip;                 // 3.579545 MHz
chip.write_address(0x20); chip.write_data(0xC7);
int16_t lr[2 * 512];
chip.generate_interleaved(lr, 512); // native rate: chip.sample_rate()
```

A C API (`nullopm/opm_c.h`) is provided for WASM and C projects; `nullopm_create_in()`
constructs a chip in caller-provided memory for MCUs.

## License

MIT
