// Noise generator: rate from NFRQ, LFSR length, routing to channel 7 C2.
#include "doctest.h"
#include "nullopm/opm.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace nullopm;

namespace {
void w(Opm& c, uint8_t a, uint8_t d) {
    c.write_address(a);
    c.write_data(d);
    c.clock();
}
void ch7_c2(Opm& c, uint8_t tl = 0) {
    w(c, 0x27, 0xC7);
    w(c, 0x2F, 0x4A);
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8 + 7);
        w(c, uint8_t(0x40 + o), 0x01);
        w(c, uint8_t(0x60 + o), slot == 3 ? tl : 0x7F);
        w(c, uint8_t(0x80 + o), 0x1F);
        w(c, uint8_t(0xE0 + o), 0x0F);
    }
    w(c, 0x08, 0x47);
}
size_t lfsr_steps(Opm& c, size_t samples) {
    size_t steps = 0;
    uint32_t prev = c.state().noise.lfsr;
    for (size_t i = 0; i < samples; ++i) {
        for (uint32_t k = 0; k < kCyclesPerSample; ++k) {
            c.clock();
            if (c.state().noise.lfsr != prev) {
                ++steps;
                prev = c.state().noise.lfsr;
            }
        }
    }
    return steps;
}
} // namespace

TEST_CASE("LFSR rate is clock / (32 * (32 - NFRQ))") {
    for (uint8_t nfrq : {0, 1, 15, 30, 31}) {
        Opm c;
        w(c, 0x0F, uint8_t(0x80 | nfrq));
        const size_t n = 8192;
        const double expect = double(n) * 2.0 / (32 - nfrq);
        CHECK(double(lfsr_steps(c, n)) == doctest::Approx(expect).epsilon(0.01));
    }
}

TEST_CASE("LFSR has the maximal period 2^17 - 1") {
    Opm c;
    w(c, 0x0F, 0x9F); // 2 steps per sample
    const uint32_t init = c.state().noise.lfsr;
    size_t steps = 0;
    bool early_repeat = false;
    uint32_t prev = init;
    while (steps < 131071) {
        c.clock();
        if (c.state().noise.lfsr != prev) {
            prev = c.state().noise.lfsr;
            ++steps;
            if (steps < 131071 && prev == init) early_repeat = true;
        }
    }
    CHECK_FALSE(early_repeat);
    CHECK(c.state().noise.lfsr == init);
}

TEST_CASE("noise replaces channel 7 C2 and follows TL") {
    auto run = [](bool ne, uint8_t tl) {
        Opm c;
        ch7_c2(c, tl);
        w(c, 0x0F, uint8_t(ne ? 0x9F : 0x1F));
        std::vector<int32_t> l(4096), r(4096);
        c.generate(l.data(), r.data(), l.size());
        return l;
    };
    auto sine = run(false, 0), noise = run(true, 0), quiet = run(true, 8);
    CHECK(sine != noise);
    // noise is a two-level signal at full scale
    int32_t hi = 0;
    size_t sign_changes = 0;
    for (size_t i = 1; i < noise.size(); ++i) {
        hi = std::max(hi, std::abs(noise[i]));
        if ((noise[i] < 0) != (noise[i - 1] < 0)) ++sign_changes;
    }
    CHECK(hi >= 8000);
    CHECK(sign_changes > 500);
    int32_t hq = 0;
    for (int32_t v : quiet) hq = std::max(hq, std::abs(v));
    CHECK(double(hi) / hq == doctest::Approx(2.0).epsilon(0.02));
    // other channels are unaffected
    Opm c;
    w(c, 0x0F, 0x9F);
    w(c, 0x20, 0xC7);
    std::vector<int32_t> l(256), r(256);
    c.generate(l.data(), r.data(), 256);
    for (int32_t v : l) CHECK(v == 0);
}
