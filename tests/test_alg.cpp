// Algorithms, feedback, pan and DAC quantization.
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
// Channel 0 with every operator ready (AR 31, RR 15, MUL 1); tl[] per slot, mask = key bits.
void setup(Opm& c, uint8_t con, uint8_t fb, uint8_t pan, const uint8_t tl[4]) {
    w(c, 0x20, uint8_t((pan << 6) | (fb << 3) | con));
    w(c, 0x28, 0x4A);
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(c, uint8_t(0x40 + o), 0x01);
        w(c, uint8_t(0x60 + o), tl[slot]);
        w(c, uint8_t(0x80 + o), 0x1F);
        w(c, uint8_t(0xE0 + o), 0x0F);
    }
    // Key on at a sample boundary so all four operators start in the same sample.
    while (c.state().cycle != 0) c.clock();
    w(c, 0x08, 0x78);
}
std::vector<int32_t> render(Opm& c, size_t n, bool right = false) {
    std::vector<int32_t> l(n), r(n);
    c.generate(l.data(), r.data(), n);
    return right ? r : l;
}
int32_t peak(const std::vector<int32_t>& v) {
    int32_t p = 0;
    for (int32_t x : v) p = std::max(p, std::abs(x));
    return p;
}
const uint8_t kOnlyC2[4] = {0x7F, 0x7F, 0x7F, 0x00};
const uint8_t kAllOn[4] = {0, 0, 0, 0};
} // namespace

TEST_CASE("pan bits route the channel to L and/or R") {
    for (uint8_t pan = 0; pan < 4; ++pan) {
        Opm c;
        setup(c, 7, 0, pan, kOnlyC2);
        std::vector<int32_t> l(512), r(512);
        c.generate(l.data(), r.data(), 512);
        CHECK((peak(l) > 0) == ((pan & 1) != 0));
        CHECK((peak(r) > 0) == ((pan & 2) != 0));
    }
}

TEST_CASE("algorithm 7 sums four carriers; muted modulators leave a pure carrier") {
    Opm one, four;
    setup(one, 7, 0, 3, kOnlyC2);
    setup(four, 7, 0, 3, kAllOn);
    auto a = render(one, 2048), b = render(four, 2048);
    // same phase for all operators -> 4x amplitude (up to DAC truncation)
    for (size_t i = 0; i < a.size(); ++i) CHECK(std::abs(b[i] - 4 * a[i]) <= 64);
}

TEST_CASE("carriers per algorithm") {
    // Number of audible carriers when all TL = 0 and modulators are silenced by
    // keying only the carriers: compare peak against a single carrier.
    const int carriers[8] = {1, 1, 1, 1, 2, 3, 3, 4};
    Opm ref;
    setup(ref, 7, 0, 3, kOnlyC2);
    const int32_t p1 = peak(render(ref, 2048));
    for (uint8_t con = 0; con < 8; ++con) {
        Opm c;
        // all TL 0, but make the modulators silent by giving them TL 0x7F unless carrier
        uint8_t tl[4];
        for (int slot = 0; slot < 4; ++slot) {
            const bool carrier = (slot == 3) || (slot == 2 && con >= 4) || (slot == 1 && con >= 5) ||
                                 (slot == 0 && con == 7);
            tl[slot] = carrier ? 0 : 0x7F;
        }
        setup(c, con, 0, 3, tl);
        const int32_t p = peak(render(c, 2048));
        CHECK(double(p) / p1 == doctest::Approx(carriers[con]).epsilon(0.02));
    }
}

TEST_CASE("modulation changes the waveform (algorithm 0 chain)") {
    Opm pure, fm;
    setup(pure, 0, 0, 3, kOnlyC2);
    const uint8_t tl[4] = {0x7F, 0x10, 0x7F, 0x00}; // M2 -> C2 is the last link of the chain
    setup(fm, 0, 0, 3, tl);
    auto a = render(pure, 4096), b = render(fm, 4096);
    CHECK(a != b);
    CHECK(peak(b) <= 8192);
    // energy of a sine vs. an FM tone of the same carrier level is similar
    double ea = 0, eb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ea += double(a[i]) * a[i];
        eb += double(b[i]) * b[i];
    }
    CHECK(eb / ea == doctest::Approx(1.0).epsilon(0.3));
}

TEST_CASE("feedback alters M1 and FB=0 does not") {
    const uint8_t tl[4] = {0x00, 0x7F, 0x7F, 0x7F}; // only M1 audible (alg 7)
    Opm fb0, fb7, ref;
    setup(fb0, 7, 0, 3, tl);
    setup(fb7, 7, 7, 3, tl);
    setup(ref, 7, 0, 3, kOnlyC2);
    auto a = render(fb0, 2048), b = render(fb7, 2048), r = render(ref, 2048);
    CHECK(a == r); // M1 without feedback is the same sine as C2
    CHECK(a != b);
    CHECK(peak(b) <= 8192);
}

TEST_CASE("DAC quantization keeps small values and truncates large ones") {
    using detail::dac_quantize;
    for (int32_t v = -511; v <= 511; ++v) CHECK(dac_quantize(v) == v);
    CHECK(dac_quantize(1023) == 1022);
    CHECK(dac_quantize(1022) == 1022);
    CHECK(dac_quantize(-1023) == -1024);
    CHECK(dac_quantize(32767) == 32704); // 6 low bits dropped
    CHECK(dac_quantize(40000) == 32704); // clamped first
    CHECK(dac_quantize(-32768) == -32768);
    CHECK(dac_quantize(0) == 0);
}
