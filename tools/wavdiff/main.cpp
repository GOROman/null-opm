// wavdiff: compare two WAV files sample by sample.  SPDX-License-Identifier: MIT
#include "wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using nullopm::tools::Wav;

static void usage() {
    std::fprintf(stderr,
                 "usage: wavdiff [options] a.wav b.wav\n"
                 "  --align N        search +-N samples of offset (cross-correlation)\n"
                 "  --normalize      fit gain/offset of b to a before comparing (analog captures)\n"
                 "  --tolerance T    exit 1 if max abs error > T (default: exact match required)\n"
                 "  --skip N         ignore the first N frames\n"
                 "  --mismatches N   print the first N mismatching frames (default 5)\n"
                 "  --csv            one-line CSV output\n"
                 "  --json           JSON output\n");
}

struct Stats {
    size_t frames = 0, exact = 0;
    double max_abs = 0;
    size_t max_at = 0;
    double rms = 0, snr_db = 0;
    long offset = 0;
    double gain = 1, dc = 0;
};

static long best_offset(const std::vector<double>& a, const std::vector<double>& b, long range) {
    long best = 0;
    double best_score = -1e300;
    const size_t n = std::min(a.size(), b.size());
    for (long off = -range; off <= range; ++off) {
        double score = 0;
        for (size_t i = 0; i < n; ++i) {
            const long j = long(i) + off;
            if (j < 0 || size_t(j) >= b.size()) continue;
            score += a[i] * b[size_t(j)];
        }
        if (score > best_score) {
            best_score = score;
            best = off;
        }
    }
    return best;
}

int main(int argc, char** argv) {
    std::string pa, pb;
    long align = 0;
    bool normalize = false, csv = false, json = false;
    double tolerance = 0;
    size_t skip = 0, show = 5;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--align") align = std::atol(next());
        else if (a == "--normalize") normalize = true;
        else if (a == "--tolerance") tolerance = std::atof(next());
        else if (a == "--skip") skip = size_t(std::atol(next()));
        else if (a == "--mismatches") show = size_t(std::atol(next()));
        else if (a == "--csv") csv = true;
        else if (a == "--json") json = true;
        else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (pa.empty()) pa = a;
        else if (pb.empty()) pb = a;
        else {
            usage();
            return 2;
        }
    }
    if (pb.empty()) {
        usage();
        return 2;
    }
    Wav wa, wb;
    std::string err;
    if (!nullopm::tools::read_wav(pa, wa, &err) || !nullopm::tools::read_wav(pb, wb, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 2;
    }
    if (wa.channels != wb.channels) {
        std::fprintf(stderr, "channel count differs (%u vs %u)\n", wa.channels, wb.channels);
        return 2;
    }
    if (wa.sample_rate != wb.sample_rate)
        std::fprintf(stderr, "warning: sample rates differ (%u vs %u)\n", wa.sample_rate, wb.sample_rate);
    const unsigned ch = wa.channels;
    // mono mix for alignment / normalization fit
    auto mix = [&](const Wav& w) {
        std::vector<double> m(w.frames());
        for (size_t i = 0; i < m.size(); ++i) {
            double s = 0;
            for (unsigned c = 0; c < ch; ++c) s += w.samples[i * ch + c];
            m[i] = s / ch;
        }
        return m;
    };
    Stats st;
    const auto ma = mix(wa), mb = mix(wb);
    if (align > 0) st.offset = best_offset(ma, mb, align);
    if (normalize) {
        // least squares b*gain + dc ~= a over the overlapping region
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        size_t n = 0;
        for (size_t i = skip; i < ma.size(); ++i) {
            const long j = long(i) + st.offset;
            if (j < 0 || size_t(j) >= mb.size()) continue;
            const double x = mb[size_t(j)], y = ma[i];
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
            ++n;
        }
        const double den = double(n) * sxx - sx * sx;
        if (n > 1 && den != 0) {
            st.gain = (double(n) * sxy - sx * sy) / den;
            st.dc = (sy - st.gain * sx) / double(n);
        }
    }
    double err2 = 0, sig2 = 0;
    std::vector<size_t> mismatches;
    for (size_t i = skip; i < wa.frames(); ++i) {
        const long j = long(i) + st.offset;
        if (j < 0 || size_t(j) >= wb.frames()) continue;
        bool frame_exact = true;
        for (unsigned c = 0; c < ch; ++c) {
            const double a = wa.samples[i * ch + c];
            const double b = wb.samples[size_t(j) * ch + c] * st.gain + st.dc;
            const double e = std::fabs(a - b);
            if (e != 0) frame_exact = false;
            if (e > st.max_abs) {
                st.max_abs = e;
                st.max_at = i;
            }
            err2 += e * e;
            sig2 += a * a;
        }
        st.frames++;
        if (frame_exact) st.exact++;
        else if (mismatches.size() < show) mismatches.push_back(i);
    }
    if (st.frames) {
        st.rms = std::sqrt(err2 / double(st.frames * ch));
        st.snr_db = err2 > 0 ? 10 * std::log10(sig2 / err2) : 999.0;
    }
    const double ratio = st.frames ? double(st.exact) / double(st.frames) : 0;
    if (csv) {
        std::printf("%zu,%zu,%.6f,%.1f,%zu,%.4f,%.2f,%ld,%.5f,%.3f\n", st.frames, st.exact, ratio,
                    st.max_abs, st.max_at, st.rms, st.snr_db, st.offset, st.gain, st.dc);
    } else if (json) {
        std::printf("{\"frames\":%zu,\"exact\":%zu,\"exact_ratio\":%.6f,\"max_abs\":%.1f,"
                    "\"max_at\":%zu,\"rms\":%.4f,\"snr_db\":%.2f,\"offset\":%ld,\"gain\":%.5f,"
                    "\"dc\":%.3f}\n",
                    st.frames, st.exact, ratio, st.max_abs, st.max_at, st.rms, st.snr_db, st.offset,
                    st.gain, st.dc);
    } else {
        std::printf("frames compared : %zu\n", st.frames);
        std::printf("sample-exact    : %zu (%.4f%%)\n", st.exact, ratio * 100);
        std::printf("max abs error   : %.0f at frame %zu\n", st.max_abs, st.max_at);
        std::printf("rms error       : %.3f\n", st.rms);
        std::printf("snr             : %.2f dB\n", st.snr_db);
        if (align) std::printf("offset          : %ld\n", st.offset);
        if (normalize) std::printf("gain / dc       : %.5f / %.3f\n", st.gain, st.dc);
        for (size_t i : mismatches) {
            std::printf("  frame %zu:", i);
            for (unsigned c = 0; c < ch; ++c)
                std::printf(" %d/%d", wa.samples[i * ch + c], wb.samples[size_t(long(i) + st.offset) * ch + c]);
            std::printf("\n");
        }
    }
    return st.max_abs > tolerance ? 1 : 0;
}
