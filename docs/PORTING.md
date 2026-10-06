# Porting to MCUs

The core is two files, `src/opm.cpp` and `src/opm_c.cpp` (optional), with
headers under `src/`. Requirements: C++17, 32-bit `int`, no exceptions/RTTI
needed, no heap, no floating point. Tables are `constexpr` and land in `.rodata`
(about 4 KB). One `Opm` instance is about 2 KB of RAM.

| Target | How |
|---|---|
| Arduino | Install this repo as a library (`library.properties` at the root); see `examples/arduino/NullOpmBasic` |
| PlatformIO | `lib_deps = https://github.com/GOROman/null-opm` (`library.json`) |
| pico-sdk | `add_subdirectory(null-opm/platform/pico)` + `target_link_libraries(app nullopm)`; see `examples/pico/hello_opm` |
| ESP-IDF | `EXTRA_COMPONENT_DIRS += null-opm/platform`; see `examples/esp-idf/hello_opm` |
| Anything else | Compile `src/opm.cpp` with `-Isrc -std=c++17 -O2` |

## CPU budget

Native rate is 55930 Hz with 32 operator slots per sample. Measured on an Apple
M-series laptop at -O2 the core renders about 40× real time (≈ 13 ns per slot).
Scale by your MCU's speed: an ESP32 at 240 MHz is roughly 30–60× slower per
clock than that laptop, so expect it to be near or below real time unless the
build is tuned. Options when the budget is tight:

- Build with `-O2`/`-O3` and keep `opm.cpp` in one translation unit (it is).
- Place the tables and `opm.cpp` in fast memory if the platform offers it
  (IRAM on ESP32).
- Render at a lower effective rate by keeping every other sample (a simple
  decimation; aliasing is audible but usable for demos).
- Run on a second core (RP2040 core 1, ESP32 app CPU) and hand samples to the
  DAC/I2S driver through a ring buffer.

Measure with the example sketches; `docs/TESTING.md` explains how to compare
output bit-exactly against the desktop build to make sure a port is unchanged.
