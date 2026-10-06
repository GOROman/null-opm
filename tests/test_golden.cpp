// Regression: render each tests/golden/inputs/*.txt and compare with the committed WAV.
// Set NULLOPM_UPDATE_GOLDEN=1 to (re)generate the goldens.
#include "doctest.h"
#include "player.hpp"
#include "vgm.hpp"
#include "wav.hpp"

#include <cstdlib>
#include <string>

using namespace nullopm::tools;

static const char* kCases[] = {"sine", "fm_epiano", "lfo_vibrato", "noise_hat"};

TEST_CASE("golden renders are unchanged") {
    const bool update = std::getenv("NULLOPM_UPDATE_GOLDEN") != nullptr;
    for (const char* name : kCases) {
        const std::string in = std::string(NULLOPM_TEST_DIR) + "/golden/inputs/" + name + ".txt";
        const std::string wav_path = std::string(NULLOPM_TEST_DIR) + "/golden/" + name + ".wav";
        RegStream s;
        std::string err;
        REQUIRE_MESSAGE(read_regstream(in, s, &err), err);
        RenderOptions opt;
        uint32_t rate = 0;
        Wav w;
        w.channels = 2;
        w.samples = render_stream(s, opt, &rate);
        w.sample_rate = rate;
        if (update) {
            REQUIRE_MESSAGE(write_wav(wav_path, w, &err), err);
            continue;
        }
        Wav g;
        REQUIRE_MESSAGE(read_wav(wav_path, g, &err), err);
        CHECK(g.sample_rate == w.sample_rate);
        REQUIRE(g.samples.size() == w.samples.size());
        size_t mismatches = 0;
        for (size_t i = 0; i < g.samples.size(); ++i) mismatches += g.samples[i] != w.samples[i];
        CHECK_MESSAGE(mismatches == 0, name);
    }
}
