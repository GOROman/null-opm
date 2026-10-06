// VGM parser and player on a synthetic file.
#include "doctest.h"
#include "player.hpp"
#include "vgm.hpp"

#include <cstdio>
#include <vector>

using namespace nullopm::tools;

namespace {
void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(uint8_t(x >> (8 * i)));
}
std::string write_temp(const std::vector<uint8_t>& bytes) {
    std::string path = std::string(NULLOPM_TEST_DIR) + "/_tmp_test.vgm";
    FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f);
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return path;
}
std::vector<uint8_t> make_vgm(bool loop) {
    std::vector<uint8_t> v(0x80, 0);
    std::memcpy(v.data(), "Vgm ", 4);
    put32(v, 0); // placeholder; fix up later
    v.resize(0x80);
    // version 1.71
    v[8] = 0x71; v[9] = 0x01;
    // YM2151 clock
    v[0x30] = 0x99; v[0x31] = 0x9E; v[0x32] = 0x36; v[0x33] = 0x00; // 3579545
    // data offset relative to 0x34 -> 0x80
    v[0x34] = 0x80 - 0x34;
    const size_t data_start = v.size();
    auto wr = [&](uint8_t a, uint8_t d) { v.insert(v.end(), {0x54, a, d}); };
    wr(0x20, 0xC7);
    wr(0x28, 0x4A);
    for (int s = 0; s < 4; ++s) {
        wr(uint8_t(0x40 + s * 8), 0x01);
        wr(uint8_t(0x60 + s * 8), s == 3 ? 0 : 0x7F);
        wr(uint8_t(0x80 + s * 8), 0x1F);
        wr(uint8_t(0xE0 + s * 8), 0x0F);
    }
    wr(0x08, 0x78);
    v.push_back(0x62); // 735
    v.push_back(0x63); // 882
    v.insert(v.end(), {0x61, 0x10, 0x27}); // 10000
    v.push_back(0x7F); // 16
    const size_t loop_pos = v.size();
    wr(0x08, 0x00);
    v.insert(v.end(), {0x61, 0x00, 0x10}); // 4096
    v.push_back(0x66);
    const uint32_t total = 735 + 882 + 10000 + 16 + 4096;
    uint32_t t = total;
    for (int i = 0; i < 4; ++i) v[0x18 + i] = uint8_t(t >> (8 * i));
    if (loop) {
        const uint32_t lo = uint32_t(loop_pos - 0x1C);
        for (int i = 0; i < 4; ++i) v[0x1C + i] = uint8_t(lo >> (8 * i));
        const uint32_t ls = 4096;
        for (int i = 0; i < 4; ++i) v[0x20 + i] = uint8_t(ls >> (8 * i));
    }
    const uint32_t eof = uint32_t(v.size() - 4);
    for (int i = 0; i < 4; ++i) v[4 + i] = uint8_t(eof >> (8 * i));
    (void)data_start;
    return v;
}
} // namespace

TEST_CASE("VGM parse: events, waits, clock and loop") {
    const std::string path = write_temp(make_vgm(true));
    RegStream s;
    std::string err;
    REQUIRE_MESSAGE(read_vgm(path, s, &err), err);
    CHECK(s.clock_hz == 3579545);
    CHECK(s.events.size() == 2 + 16 + 1 + 1);
    CHECK(s.events[0].time == 0);
    CHECK(s.events.back().time == 735 + 882 + 10000 + 16);
    CHECK(s.events.back().addr == 0x08);
    CHECK(s.total_time == 735 + 882 + 10000 + 16 + 4096);
    CHECK(s.has_loop);
    CHECK(s.loop_time == 735 + 882 + 10000 + 16);
    std::remove(path.c_str());
}

TEST_CASE("render: duration matches, sound then silence, loop count") {
    const std::string path = write_temp(make_vgm(true));
    RegStream s;
    REQUIRE(read_vgm(path, s));
    RenderOptions opt;
    uint32_t rate = 0, dropped = 1;
    auto pcm = render_stream(s, opt, &rate, &dropped);
    CHECK(rate == 55930);
    CHECK(dropped == 0);
    const double expect_frames = double(s.total_time) * 55930.0 / 44100.0;
    CHECK(double(pcm.size() / 2) == doctest::Approx(expect_frames).epsilon(0.0001));
    // audible in the middle of the note, silent (released) at the end
    const size_t mid = size_t(5000.0 * 55930 / 44100) * 2;
    int16_t peak = 0;
    for (size_t i = mid; i < mid + 400; i += 2) peak = std::max<int16_t>(peak, int16_t(std::abs(pcm[i])));
    CHECK(peak > 4000);
    CHECK(pcm[pcm.size() - 2] == 0);
    opt.loops = 3;
    auto pcm3 = render_stream(s, opt, &rate);
    const double expect3 = double(s.total_time + 2 * 4096) * 55930.0 / 44100.0;
    CHECK(double(pcm3.size() / 2) == doctest::Approx(expect3).epsilon(0.0001));
    std::remove(path.c_str());
}

TEST_CASE("regdump round trip") {
    const std::string path = write_temp(make_vgm(false));
    RegStream s, s2;
    REQUIRE(read_vgm(path, s));
    const std::string txt = std::string(NULLOPM_TEST_DIR) + "/_tmp_test.txt";
    REQUIRE(write_regdump(txt, s));
    REQUIRE(read_regstream(txt, s2));
    CHECK(s2.events.size() == s.events.size());
    CHECK(s2.total_time == s.total_time);
    CHECK(s2.clock_hz == s.clock_hz);
    for (size_t i = 0; i < s.events.size(); ++i) {
        CHECK(s2.events[i].time == s.events[i].time);
        CHECK(s2.events[i].addr == s.events[i].addr);
        CHECK(s2.events[i].data == s.events[i].data);
    }
    std::remove(path.c_str());
    std::remove(txt.c_str());
}
