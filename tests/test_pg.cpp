// Phase generator: KC/KF/DT1/DT2/MUL -> frequency, measured on the rendered output.
#include "doctest.h"
#include "nullopm/opm.hpp"
#include "../src/opm_tables.hpp"

#include <cmath>
#include <vector>

using namespace nullopm;

namespace {
void w(Opm& c, uint8_t a, uint8_t d) {
    c.write_address(a);
    c.write_data(d);
    c.clock();
}
// Single carrier on channel 0 slot C2 (op index 24), TL 0, gate envelope.
void setup_single(Opm& c, uint8_t kc, uint8_t kf, uint8_t dt1mul = 0x01, uint8_t dt2 = 0) {
    w(c, 0x20, 0xC7);
    w(c, 0x28, kc);
    w(c, 0x30, uint8_t(kf << 2));
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(c, uint8_t(0x40 + o), slot == 3 ? dt1mul : 0x01);
        w(c, uint8_t(0x60 + o), slot == 3 ? 0x00 : 0x7F);
        w(c, uint8_t(0x80 + o), 0x1F); // AR=31
        w(c, uint8_t(0xC0 + o), uint8_t(dt2 << 6));
        w(c, uint8_t(0xE0 + o), 0x0F);
    }
    w(c, 0x08, 0x40); // key on C2 only
}
// Frequency from rising zero crossings over n samples.
double measure_hz(Opm& c, size_t n) {
    std::vector<int32_t> l(n), r(n);
    c.generate(l.data(), r.data(), n);
    size_t first = 0, last = 0, count = 0;
    for (size_t i = 1; i < n; ++i) {
        if (l[i - 1] < 0 && l[i] >= 0) {
            if (count == 0) first = i;
            last = i;
            ++count;
        }
    }
    if (count < 2) return 0;
    return double(count - 1) * c.sample_rate() / double(last - first);
}
double expected_hz(int octave, int semi, int kf) {
    return 440.0 / std::pow(2.0, 4 + 8.0 / 12.0) * std::pow(2.0, octave + (semi * 64 + kf) / 768.0);
}
} // namespace

TEST_CASE("KC 0x4A is A4 = 440 Hz") {
    Opm c;
    setup_single(c, 0x4A, 0);
    CHECK(measure_hz(c, 55930 * 2) == doctest::Approx(440.0).epsilon(0.001));
}

TEST_CASE("octaves and notes follow the KC table") {
    const struct { uint8_t kc; int oct, semi; } cases[] = {
        {0x00, 0, 0}, {0x0E, 0, 11}, {0x2A, 2, 8}, {0x4A, 4, 8}, {0x6E, 6, 11}, {0x70, 7, 0},
        {0x43, 4, 3}, {0x44, 4, 3}, // code 3 duplicates code 4 (E)
    };
    for (auto& t : cases) {
        Opm c;
        setup_single(c, t.kc, 0);
        const double hz = expected_hz(t.oct, t.semi, 0);
        CHECK(measure_hz(c, size_t(55930 * 4)) == doctest::Approx(hz).epsilon(0.004));
    }
}

TEST_CASE("KF raises pitch by 1/64 semitone steps") {
    Opm c;
    setup_single(c, 0x4A, 32); // half a semitone up
    CHECK(measure_hz(c, 55930 * 2) == doctest::Approx(expected_hz(4, 8, 32)).epsilon(0.001));
}

TEST_CASE("MUL multiplies, MUL=0 halves") {
    Opm a, b;
    setup_single(a, 0x4A, 0, 0x02);
    setup_single(b, 0x4A, 0, 0x00);
    CHECK(measure_hz(a, 55930 * 2) == doctest::Approx(880.0).epsilon(0.001));
    CHECK(measure_hz(b, 55930 * 2) == doctest::Approx(220.0).epsilon(0.001));
}

TEST_CASE("DT2 adds 600 / 781 / 950 cents") {
    const double cents[4] = {0, 600, 781.25, 950};
    for (int d = 0; d < 4; ++d) {
        Opm c;
        setup_single(c, 0x4A, 0, 0x01, uint8_t(d));
        const double hz = 440.0 * std::pow(2.0, cents[d] / 1200.0);
        CHECK(measure_hz(c, 55930 * 2) == doctest::Approx(hz).epsilon(0.002));
    }
}

TEST_CASE("DT1 shifts the phase increment by the datasheet LSB count") {
    // Inspect the computed increment directly: KC 0x4A -> key code 0x12.
    Opm base, plus, minus;
    setup_single(base, 0x4A, 0, 0x01);
    setup_single(plus, 0x4A, 0, 0x31);  // DT1=3
    setup_single(minus, 0x4A, 0, 0x71); // DT1=-3
    const uint32_t i0 = base.state().op[24].phase_inc;
    const uint32_t dt = tables::kDt1[3][0x12];
    CHECK(plus.state().op[24].phase_inc == i0 + dt);
    CHECK(minus.state().op[24].phase_inc == i0 - dt);
}
