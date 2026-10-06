// LFO frequency, waveforms, PMS / AMS depth and the reset bit.
#include "doctest.h"
#include "nullopm/opm.hpp"

#include <cmath>
#include <vector>

using namespace nullopm;

namespace {
void w(Opm& c, uint8_t a, uint8_t d) {
    c.write_address(a);
    c.write_data(d);
    c.clock();
}
void step(Opm& c, size_t n = 1) {
    int32_t l, r;
    for (size_t i = 0; i < n; ++i) c.generate(&l, &r, 1);
}
// LFO cycles per second measured from phase wraps.
double lfo_hz(Opm& c, size_t samples) {
    uint8_t prev = c.state().lfo.phase;
    size_t wraps = 0, first = 0, last = 0;
    for (size_t i = 0; i < samples; ++i) {
        step(c);
        const uint8_t p = c.state().lfo.phase;
        if (p < prev) {
            if (!wraps) first = i;
            last = i;
            ++wraps;
        }
        prev = p;
    }
    return wraps > 1 ? double(wraps - 1) * 55930.0 / double(last - first) : 0;
}
void single_carrier(Opm& c, uint8_t pms, uint8_t ams, bool ame) {
    w(c, 0x20, 0xC7);
    w(c, 0x28, 0x4A);
    w(c, 0x38, uint8_t((pms << 4) | ams));
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(c, uint8_t(0x40 + o), 0x01);
        w(c, uint8_t(0x60 + o), slot == 3 ? 0x00 : 0x7F);
        w(c, uint8_t(0x80 + o), 0x1F);
        w(c, uint8_t(0xA0 + o), ame ? 0x80 : 0x00);
        w(c, uint8_t(0xE0 + o), 0x0F);
    }
    w(c, 0x08, 0x40);
}
} // namespace

TEST_CASE("LFRQ 0xFF is about 32.6 Hz and each exponent step doubles") {
    Opm c;
    w(c, 0x18, 0xFF);
    CHECK(lfo_hz(c, 55930 * 2) == doctest::Approx(32.6).epsilon(0.02));
    Opm d;
    w(d, 0x18, 0xEF);
    CHECK(lfo_hz(d, 55930 * 2) == doctest::Approx(16.3).epsilon(0.02));
    Opm e; // mantissa: 0xF0 = 16/31 of 0xFF
    w(e, 0x18, 0xF0);
    CHECK(lfo_hz(e, 55930 * 4) == doctest::Approx(32.6 * 16 / 31).epsilon(0.02));
}

TEST_CASE("waveform shapes: PM sign and AM range") {
    for (uint8_t wave = 0; wave < 3; ++wave) {
        Opm c;
        w(c, 0x18, 0xFF);
        w(c, 0x1B, wave);
        w(c, 0x19, 0xFF); // PMD 127
        w(c, 0x19, 0x7F); // AMD 127
        int16_t pm_min = 0, pm_max = 0;
        uint8_t am_min = 255, am_max = 0;
        for (int i = 0; i < 4000; ++i) {
            step(c);
            pm_min = std::min(pm_min, c.state().lfo.pm);
            pm_max = std::max(pm_max, c.state().lfo.pm);
            am_min = std::min(am_min, c.state().lfo.am);
            am_max = std::max(am_max, c.state().lfo.am);
        }
        CHECK(pm_max >= 120);
        CHECK(pm_min <= -120);
        CHECK(am_min == 0);
        CHECK(am_max >= 120);
    }
}

TEST_CASE("PMS 7 with square PM swings the pitch by about +-700 cents") {
    Opm c;
    single_carrier(c, 7, 0, false);
    w(c, 0x18, 0x00); // slowest LFO so the square level is stable
    w(c, 0x1B, 0x01); // square
    const uint32_t base = c.state().op[24].phase_inc;
    w(c, 0x19, 0xFF); // PMD 127
    step(c, 4);
    const uint32_t up = c.state().op[24].phase_inc;
    const double cents = 1200.0 * std::log2(double(up) / base);
    CHECK(cents == doctest::Approx(700).epsilon(0.05));
}

TEST_CASE("PMS table scales the deviation") {
    const double expect[8] = {0, 5, 10, 20, 50, 100, 400, 700};
    for (uint8_t pms = 1; pms < 8; ++pms) {
        Opm c;
        single_carrier(c, pms, 0, false);
        w(c, 0x1B, 0x01);
        const uint32_t base = c.state().op[24].phase_inc;
        w(c, 0x19, 0xFF);
        step(c, 4);
        const double cents = 1200.0 * std::log2(double(c.state().op[24].phase_inc) / base);
        // 1/64-semitone granularity = 1.56 cents
        CHECK(std::abs(cents - expect[pms]) <= std::max(3.2, expect[pms] * 0.1));
    }
}

TEST_CASE("AMS depth: square AM attenuates the second half by 1.4 / 5.9 / 11.8 dB") {
    const double db[4] = {0, 1.4, 5.9, 11.8};
    for (uint8_t ams = 1; ams < 4; ++ams) {
        Opm c;
        single_carrier(c, 0, ams, true);
        w(c, 0x18, 0xE0); // ~8 Hz: half cycle ~3500 samples
        w(c, 0x1B, 0x01);
        w(c, 0x19, 0x7F); // AMD 127
        std::vector<int32_t> l(55930 / 2), r(l.size());
        c.generate(l.data(), r.data(), l.size());
        // collect peaks in windows and take the max and min window peak
        int32_t hi = 0, lo = 1 << 30;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) {
            int32_t p = 0;
            for (size_t j = i; j < i + 256; ++j) p = std::max(p, std::abs(l[j]));
            hi = std::max(hi, p);
            lo = std::min(lo, p);
        }
        const double measured = 20 * std::log10(double(hi) / lo);
        CHECK(measured == doctest::Approx(db[ams]).epsilon(0.1));
    }
}

TEST_CASE("LFO reset bit holds the phase at zero") {
    Opm c;
    w(c, 0x18, 0xFF);
    w(c, 0x01, 0x02);
    step(c, 5000);
    CHECK(c.state().lfo.phase == 0);
    w(c, 0x01, 0x00);
    step(c, 5000);
    CHECK(c.state().lfo.phase != 0);
}

TEST_CASE("CT1/CT2 callback fires on change") {
    Opm c;
    static uint8_t seen;
    static int calls;
    seen = 0;
    calls = 0;
    Callbacks cb;
    cb.ct = [](void*, uint8_t bits) {
        seen = bits;
        ++calls;
    };
    c.set_callbacks(cb);
    w(c, 0x1B, 0x80);
    CHECK(c.ct() == 2);
    CHECK(seen == 2);
    w(c, 0x1B, 0x80); // same value: no call
    CHECK(calls == 1);
    w(c, 0x1B, 0x40);
    CHECK(c.ct() == 1);
    CHECK(calls == 2);
}
