// vgm2wav: render a YM2151 VGM / register log to WAV with null-opm.
// SPDX-License-Identifier: MIT
#include "player.hpp"
#include "resampler.hpp"
#include "vgm.hpp"
#include "wav.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void usage() {
    std::fprintf(stderr,
                 "usage: vgm2wav [options] input.vgm|input.txt output.wav\n"
                 "  --rate HZ       resample output (default: native clock/64)\n"
                 "  --clock HZ      override chip clock\n"
                 "  --loops N       play the loop section N times (default 1)\n"
                 "  --seconds S     stop after S seconds\n"
                 "  --tail S        append S seconds after the end (default 0)\n"
                 "  --regdump FILE  also write the register log as text\n");
}

int main(int argc, char** argv) {
    nullopm::tools::RenderOptions opt;
    uint32_t out_rate = 0;
    std::string in, out, regdump;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--rate") out_rate = uint32_t(std::atoi(next()));
        else if (a == "--clock") opt.clock_hz = uint32_t(std::atoi(next()));
        else if (a == "--loops") opt.loops = std::atoi(next());
        else if (a == "--seconds") opt.max_seconds = std::atof(next());
        else if (a == "--tail") opt.tail_seconds = std::atof(next());
        else if (a == "--regdump") regdump = next();
        else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (in.empty()) in = a;
        else if (out.empty()) out = a;
        else {
            usage();
            return 2;
        }
    }
    if (in.empty() || out.empty()) {
        usage();
        return 2;
    }
    nullopm::tools::RegStream stream;
    std::string err;
    if (!nullopm::tools::read_regstream(in, stream, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    if (!stream.gd3_title.empty()) std::printf("title: %s\n", stream.gd3_title.c_str());
    std::printf("events: %zu, length: %.2f s%s, clock: %u Hz\n", stream.events.size(),
                double(stream.total_time) / 44100.0, stream.has_loop ? " (loops)" : "",
                stream.clock_hz ? stream.clock_hz : nullopm::kDefaultClockHz);
    if (!regdump.empty() && !nullopm::tools::write_regdump(regdump, stream, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    uint32_t native = 0, dropped = 0;
    auto pcm = nullopm::tools::render_stream(stream, opt, &native, &dropped);
    if (dropped) std::fprintf(stderr, "warning: %u writes arrived while busy and were dropped\n", dropped);
    nullopm::tools::Wav wav;
    wav.channels = 2;
    wav.sample_rate = native;
    if (out_rate && out_rate != native) {
        pcm = nullopm::tools::resample(pcm, 2, native, out_rate);
        wav.sample_rate = out_rate;
    }
    wav.samples = std::move(pcm);
    if (!nullopm::tools::write_wav(out, wav, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::printf("wrote %s: %zu frames @ %u Hz\n", out.c_str(), wav.frames(), wav.sample_rate);
    return 0;
}
