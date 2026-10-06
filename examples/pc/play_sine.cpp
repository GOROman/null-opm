// Minimal example: key on a single sine-like operator and write a WAV file.
// SPDX-License-Identifier: MIT
#include "nullopm/opm.hpp"
#include "wav.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "sine.wav";
    nullopm::Opm chip;
    auto w = [&](uint8_t a, uint8_t d) {
        chip.write_address(a);
        chip.write_data(d);
        chip.clock();
    };
    // Channel 0, algorithm 7 (all carriers), only C2 audible (TL=0), others muted.
    w(0x20, 0xC7); // R L FB=0 CON=7
    w(0x28, 0x4A); // KC: octave 4, A -> 440 Hz
    w(0x30, 0x00); // KF
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(uint8_t(0x40 + o), 0x01); // DT1=0 MUL=1
        w(uint8_t(0x60 + o), slot == 3 ? 0x00 : 0x7F);
        w(uint8_t(0x80 + o), 0x1F); // KS=0 AR=31
        w(uint8_t(0xA0 + o), 0x00); // D1R=0
        w(uint8_t(0xC0 + o), 0x00); // DT2=0 D2R=0
        w(uint8_t(0xE0 + o), 0x0F); // D1L=0 RR=15
    }
    w(0x08, 0x78); // key on all slots, channel 0

    nullopm::tools::Wav wav;
    wav.sample_rate = chip.sample_rate();
    wav.channels = 2;
    const size_t frames = wav.sample_rate; // 1 second
    wav.samples.resize(frames * 2);
    chip.generate_interleaved(wav.samples.data(), frames);
    std::string err;
    if (!nullopm::tools::write_wav(out, wav, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::printf("wrote %s (%zu frames @ %u Hz)\n", out, frames, wav.sample_rate);
    return 0;
}
