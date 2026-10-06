// Envelope generator timing, derived from the datasheet rate structure:
// each +4 in effective rate halves the time; rate = 2*R + keycode >> (3 - KS).
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
constexpr int kOp = 24; // channel 0, slot C2
struct Env {
    uint8_t ar = 31, d1r = 0, d2r = 0, rr = 15, d1l = 0, ks = 0, tl = 0, kc = 0x4A;
};
void setup(Opm& c, const Env& e) {
    w(c, 0x20, 0xC7);
    w(c, 0x28, e.kc);
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(c, uint8_t(0x40 + o), 0x01);
        w(c, uint8_t(0x60 + o), slot == 3 ? e.tl : 0x7F);
        w(c, uint8_t(0x80 + o), uint8_t((e.ks << 6) | e.ar));
        w(c, uint8_t(0xA0 + o), e.d1r);
        w(c, uint8_t(0xC0 + o), e.d2r);
        w(c, uint8_t(0xE0 + o), uint8_t((e.d1l << 4) | e.rr));
    }
}
void key_on(Opm& c) { w(c, 0x08, 0x40); }
void key_off(Opm& c) { w(c, 0x08, 0x00); }
uint16_t level(const Opm& c) { return c.state().op[kOp].eg_level; }
// Samples until the predicate holds (max limit).
template <class P> size_t samples_until(Opm& c, size_t limit, P pred) {
    int32_t l, r;
    for (size_t i = 0; i < limit; ++i) {
        if (pred(c)) return i;
        c.generate(&l, &r, 1);
    }
    return limit;
}
// Expected samples for a 10-bit sweep of `steps` at effective rate r.
double expected_samples(uint32_t r, double steps) {
    if (r >= 48) {
        const double per_sample = std::pow(2.0, (r - 48) >> 2) * (4 + (r & 3)) / 4.0;
        return steps / per_sample;
    }
    const double shift = 11 - (r >> 2);
    const double per_step = (4 + (r & 3)) / 8.0;
    return steps * std::pow(2.0, shift) / per_step;
}
} // namespace

TEST_CASE("AR=31 reaches full level immediately") {
    Opm c;
    setup(c, Env{});
    key_on(c);
    int32_t l, r;
    c.generate(&l, &r, 1);
    CHECK(level(c) == 0);
}

TEST_CASE("decay time halves every +4 effective rate (D1R, KS=0)") {
    for (uint8_t d1r : {4, 8, 12, 16, 20, 24, 28}) {
        Opm c;
        Env e;
        e.d1r = d1r;
        e.d1l = 15; // decay all the way to -93 dB
        setup(c, e);
        key_on(c);
        const uint32_t rate = 2u * d1r + (0x12 >> 3); // keycode 0x12 >> 3 = 2
        const double expect = expected_samples(rate, 992);
        const size_t n = samples_until(c, size_t(expect * 4) + 100,
                                       [](const Opm& o) { return level(o) >= 992; });
        CHECK(double(n) == doctest::Approx(expect).epsilon(0.05));
    }
}

TEST_CASE("key scaling raises the effective rate") {
    Opm a, b;
    Env e;
    e.d1r = 8;
    e.d1l = 15;
    setup(a, e);
    e.ks = 3;
    setup(b, e);
    key_on(a);
    key_on(b);
    const size_t na = samples_until(a, 1u << 20, [](const Opm& o) { return level(o) >= 992; });
    const size_t nb = samples_until(b, 1u << 20, [](const Opm& o) { return level(o) >= 992; });
    // KS=3 adds the whole 5-bit keycode (0x12 = 18) instead of keycode >> 3 (2): +16 = x16 faster
    CHECK(double(na) / double(nb) == doctest::Approx(16.0).epsilon(0.1));
}

TEST_CASE("D1L holds the level when D2R = 0") {
    Opm c;
    Env e;
    e.d1r = 20;
    e.d1l = 5; // 5 * 32 = 160
    setup(c, e);
    key_on(c);
    int32_t l[4096], r[4096];
    c.generate(l, r, 4096);
    CHECK(level(c) == 160);
    c.generate(l, r, 4096);
    CHECK(level(c) == 160);
}

TEST_CASE("release goes to 1023 at rate 2*(2*RR+1)") {
    Opm c;
    Env e;
    e.rr = 4;
    setup(c, e);
    key_on(c);
    int32_t l[16], r[16];
    c.generate(l, r, 16);
    key_off(c);
    const uint32_t rate = 2u * (2 * 4 + 1) + 2;
    const double expect = expected_samples(rate, 1023);
    const size_t n = samples_until(c, size_t(expect * 4) + 100,
                                   [](const Opm& o) { return level(o) >= 1023; });
    CHECK(double(n) == doctest::Approx(expect).epsilon(0.05));
}

TEST_CASE("attack time is finite and scales with AR") {
    size_t prev = 0;
    for (uint8_t ar : {8, 12, 16}) {
        Opm c;
        Env e;
        e.ar = ar;
        setup(c, e);
        key_on(c);
        const size_t n = samples_until(c, 1u << 22, [](const Opm& o) { return level(o) == 0; });
        CHECK(n < (1u << 22));
        if (prev) CHECK(double(prev) / double(n) == doctest::Approx(4.0).epsilon(0.15));
        prev = n;
    }
}

TEST_CASE("TL attenuates by 0.75 dB per step") {
    auto peak = [](uint8_t tl) {
        Opm c;
        Env e;
        e.tl = tl;
        setup(c, e);
        key_on(c);
        std::vector<int32_t> l(1024), r(1024);
        c.generate(l.data(), r.data(), l.size());
        int32_t p = 0;
        for (int32_t v : l) p = std::max(p, std::abs(v));
        return p;
    };
    const int32_t p0 = peak(0);
    CHECK(double(p0) / peak(8) == doctest::Approx(2.0).epsilon(0.01));   // -6 dB
    CHECK(double(p0) / peak(16) == doctest::Approx(4.0).epsilon(0.01));  // -12 dB
    CHECK(peak(0x7F) <= 1);                                              // -95 dB
}
