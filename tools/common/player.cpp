// SPDX-License-Identifier: MIT
#include "player.hpp"

namespace nullopm::tools {

namespace {
struct Sink {
    Opm& chip;
    std::vector<int16_t>& out;
    uint64_t cycles = 0;
    void clock_once() {
        chip.clock();
        ++cycles;
        if (chip.state().cycle == 0) {
            const auto& st = chip.state();
            auto sat = [](int32_t v) {
                return int16_t(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
            };
            out.push_back(sat(st.out_l));
            out.push_back(sat(st.out_r));
        }
    }
    void run_to(uint64_t target_cycles) {
        while (cycles < target_cycles) clock_once();
    }
};
} // namespace

std::vector<int16_t> render_stream(const RegStream& s, const RenderOptions& opt,
                                   uint32_t* out_rate, uint32_t* dropped) {
    const uint32_t clock = opt.clock_hz ? opt.clock_hz : (s.clock_hz ? s.clock_hz : kDefaultClockHz);
    Opm chip(clock);
    std::vector<int16_t> out;
    Sink sink{chip, out};
    const double cycles_per_vgm = double(clock) / 2.0 / 44100.0; // phi1 cycles per 1/44100 s
    auto to_cycles = [&](uint64_t t) { return uint64_t(double(t) * cycles_per_vgm); };
    const uint64_t max_cycles = opt.max_seconds > 0 ? uint64_t(opt.max_seconds * clock / 2) : ~0ull;

    const int passes = s.has_loop ? opt.loops : 1;
    uint64_t time_base = 0; // VGM time offset of the current pass
    for (int pass = 0; pass < passes; ++pass) {
        size_t start = 0;
        uint64_t pass_start_time = 0;
        if (pass > 0) {
            pass_start_time = s.loop_time;
            while (start < s.events.size() && s.events[start].time < s.loop_time) ++start;
        }
        for (size_t i = start; i < s.events.size(); ++i) {
            const auto& e = s.events[i];
            const uint64_t target = to_cycles(time_base + e.time - pass_start_time);
            if (target > max_cycles) break;
            sink.run_to(target);
            chip.write_address(e.addr);
            sink.clock_once();
            chip.write_data(e.data);
            sink.clock_once();
        }
        const uint64_t end = to_cycles(time_base + s.total_time - pass_start_time);
        if (end > max_cycles) {
            sink.run_to(max_cycles);
            break;
        }
        sink.run_to(end);
        time_base += s.total_time - pass_start_time;
    }
    if (opt.tail_seconds > 0) sink.run_to(sink.cycles + uint64_t(opt.tail_seconds * clock / 2));
    if (out_rate) *out_rate = chip.sample_rate();
    if (dropped) *dropped = chip.dropped_writes();
    return out;
}

} // namespace nullopm::tools
