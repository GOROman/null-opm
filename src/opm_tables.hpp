// null-opm: compile-time lookup tables.  SPDX-License-Identifier: MIT
//
// All tables are generated with constexpr code from their mathematical
// definition (see docs/ARCHITECTURE.md). Nothing here is copied from any ROM
// dump or emulator; test_tables.cpp verifies the generated values against the
// same definitions evaluated with <cmath> at run time.
#ifndef NULLOPM_OPM_TABLES_HPP
#define NULLOPM_OPM_TABLES_HPP

#include <cstdint>

namespace nullopm::tables {

// ---- constexpr math helpers (double precision, enough for 13-bit tables) ----
namespace cm {
constexpr double kPi = 3.14159265358979323846;
constexpr double kLn2 = 0.69314718055994530942;

constexpr double abs(double x) { return x < 0 ? -x : x; }
constexpr double floor(double x) {
    const long long i = static_cast<long long>(x);
    return (x < 0 && static_cast<double>(i) != x) ? static_cast<double>(i - 1)
                                                   : static_cast<double>(i);
}
constexpr double round(double x) { return floor(x + 0.5); }

// sin via Taylor series after reducing to [-pi, pi]
constexpr double sin(double x) {
    while (x > kPi) x -= 2 * kPi;
    while (x < -kPi) x += 2 * kPi;
    double term = x, sum = x;
    const double x2 = x * x;
    for (int n = 1; n < 20; ++n) {
        term *= -x2 / static_cast<double>((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

// natural log via atanh series with power-of-two range reduction
constexpr double log(double x) {
    int e = 0;
    while (x >= 2.0) { x *= 0.5; ++e; }
    while (x < 1.0) { x *= 2.0; --e; }
    const double y = (x - 1) / (x + 1);
    const double y2 = y * y;
    double term = y, sum = 0;
    for (int n = 1; n < 60; n += 2) {
        sum += term / static_cast<double>(n);
        term *= y2;
    }
    return 2 * sum + static_cast<double>(e) * kLn2;
}
constexpr double log2(double x) { return log(x) / kLn2; }

// e^x via Taylor series with range reduction
constexpr double exp(double x) {
    const int k = static_cast<int>(floor(x / kLn2 + 0.5));
    const double r = x - static_cast<double>(k) * kLn2;
    double term = 1, sum = 1;
    for (int n = 1; n < 30; ++n) {
        term *= r / static_cast<double>(n);
        sum += term;
    }
    double p = 1;
    for (int i = 0; i < (k < 0 ? -k : k); ++i) p *= 2;
    return k < 0 ? sum / p : sum * p;
}
constexpr double pow2(double x) { return exp(x * kLn2); }
} // namespace cm

// ---- log-sin: 256-entry quarter wave, -log2(sin) * 256 ------------------
// Index i covers angles ((i + 0.5) / 256) * (pi / 2). Output is 12-bit.
struct LogSinTable {
    uint16_t v[256]{};
    constexpr LogSinTable() {
        for (int i = 0; i < 256; ++i) {
            const double s = cm::sin((static_cast<double>(i) + 0.5) * cm::kPi / 512.0);
            v[i] = static_cast<uint16_t>(cm::round(-cm::log2(s) * 256.0));
        }
    }
};
inline constexpr LogSinTable kLogSin{};

// ---- exp: 256-entry (2^(i/256) - 1) * 1024 ------------------------------
// Looked up with the inverted low 8 bits of the attenuation; bit 10 is implied.
struct ExpTable {
    uint16_t v[256]{};
    constexpr ExpTable() {
        for (int i = 0; i < 256; ++i)
            v[i] = static_cast<uint16_t>(
                cm::round((cm::pow2(static_cast<double>(i) / 256.0) - 1.0) * 1024.0));
    }
};
inline constexpr ExpTable kExp{};

// ---- F-number: 768 entries, one per 1/64 semitone, octave 0 ---------------
// Phase accumulator is 20 bits; at 3.579545 MHz (fs = 55930 Hz) KC=0x4A is A4 =
// 440 Hz (datasheet KC table), so octave 0 starts at C#0 = 440 / 2^(4 + 8/12).
// Stored with 8 fractional bits: inc20 = (kFnum[frac] << octave) >> 8.
constexpr int kFnumEntries = 768;
constexpr int kFnumFracBits = 8;
struct FnumTable {
    uint32_t v[kFnumEntries]{};
    constexpr FnumTable() {
        // C#0 increment in 20-bit phase units per sample, times 2^8.
        // 440 Hz / 2^(4 + 8/12) * 2^20 / 55930 * 256
        const double csharp0 = 440.0 / cm::pow2(4.0 + 8.0 / 12.0);
        const double base = csharp0 * 1048576.0 / 55930.0 * 256.0;
        for (int i = 0; i < kFnumEntries; ++i)
            v[i] = static_cast<uint32_t>(
                cm::round(base * cm::pow2(static_cast<double>(i) / 768.0)));
    }
};
inline constexpr FnumTable kFnum{};

// ---- DT2: coarse detune in 1/64-semitone units ------------------------------
// Datasheet DT2 table: 0 / 600 / 781 / 950 cents.
inline constexpr uint16_t kDt2[4] = {0, 384, 500, 608};

// ---- DT1: fine detune in phase-increment LSBs, per 5-bit key code ----------
// Datasheet DT1 table (Hz at 3.58 MHz) divided by the LSB value fs / 2^20.
inline constexpr uint8_t kDt1[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7},
    {1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16},
    {2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22},
};

} // namespace nullopm::tables

#endif // NULLOPM_OPM_TABLES_HPP
