// Verify compile-time tables against their definitions evaluated with <cmath>.
#include "doctest.h"
#include "../src/opm_tables.hpp"

#include <cmath>

using namespace nullopm::tables;

TEST_CASE("log-sin table matches -log2(sin) * 256") {
    for (int i = 0; i < 256; ++i) {
        const double s = std::sin((i + 0.5) * M_PI / 512.0);
        const auto expect = static_cast<uint16_t>(std::lround(-std::log2(s) * 256.0));
        CHECK(kLogSin.v[i] == expect);
    }
    CHECK(kLogSin.v[255] == 0);  // sin ~ 1 near pi/2
    CHECK(kLogSin.v[0] > 2000);  // very small sin -> large attenuation
    for (int i = 1; i < 256; ++i) CHECK(kLogSin.v[i] <= kLogSin.v[i - 1]); // monotonic
}

TEST_CASE("exp table matches (2^(i/256) - 1) * 1024") {
    for (int i = 0; i < 256; ++i) {
        const auto expect = static_cast<uint16_t>(std::lround((std::pow(2.0, i / 256.0) - 1) * 1024));
        CHECK(kExp.v[i] == expect);
    }
    CHECK(kExp.v[0] == 0);
    CHECK(kExp.v[255] == 1018);
}

TEST_CASE("fnum table: A4 (octave 4, 8 semitones + 0 KF) gives 440 Hz") {
    const double fs = 55930.0;
    const uint32_t inc = (kFnum.v[8 * 64] << 4) >> kFnumFracBits;
    const double hz = inc * fs / 1048576.0;
    CHECK(hz == doctest::Approx(440.0).epsilon(0.0005));
    // one octave = exactly double
    CHECK(kFnum.v[767] < 2 * kFnum.v[0]);
    for (int i = 1; i < kFnumEntries; ++i) CHECK(kFnum.v[i] >= kFnum.v[i - 1]);
}

TEST_CASE("DT1 table is the datasheet table (Hz / 0.0533)") {
    // Datasheet: DT1=1, key code 0x1F (highest) is about 0.373 Hz -> 7 LSB
    CHECK(kDt1[1][31] == 7);
    CHECK(kDt1[3][28] == 22);
    for (int d = 1; d < 4; ++d)
        for (int k = 1; k < 32; ++k) CHECK(kDt1[d][k] >= kDt1[d][k - 1]);
}
