// null-opm: clean-room YM2151 (OPM) emulator core.
// SPDX-License-Identifier: MIT
//
// Clean-room notice: this file is written from the YM2151 / YM3012 datasheets,
// the YM2151 application manual and general FM synthesis knowledge only.
// No emulator source code was consulted. See docs/CLEANROOM.md.
#include "nullopm/opm.hpp"

#include <cstring>

namespace nullopm {

const char* version() noexcept { return "0.1.0"; }

Opm::Opm(uint32_t clock_hz) noexcept {
    s_.clock_hz = clock_hz;
    reset();
}

void Opm::set_callbacks(const Callbacks& cb) noexcept { s_.cb = cb; }

void Opm::reset() noexcept {
    const Callbacks cb = s_.cb;
    const uint32_t clk = s_.clock_hz;
    s_ = detail::State{};
    s_.cb = cb;
    s_.clock_hz = clk;
}

// --- bus ---------------------------------------------------------------

void Opm::write(uint8_t a0, uint8_t data) noexcept {
    if (a0 & 1) write_data(data);
    else write_address(data);
}

void Opm::write_address(uint8_t addr) noexcept { s_.address = addr; }

void Opm::write_data(uint8_t data) noexcept {
    // The chip latches one pending write; it is applied at the next internal cycle
    // and the busy flag is raised for a fixed time. A write arriving while another
    // is still pending replaces it (the first write is lost on real hardware).
    if (s_.latch.pending) s_.latch.dropped++;
    s_.latch.addr = s_.address;
    s_.latch.data = data;
    s_.latch.pending = 1;
    s_.latch.busy = kBusyCycles;
}

uint8_t Opm::read_status() const noexcept {
    uint8_t st = s_.timers.status & (status::kTimerA | status::kTimerB);
    if (s_.latch.busy) st |= status::kBusy;
    return st;
}

void Opm::set_irq(bool on) noexcept {
    const uint8_t v = on ? 1 : 0;
    if (v == s_.irq) return;
    s_.irq = v;
    if (s_.cb.irq) s_.cb.irq(s_.cb.user, on);
}

// --- register commit -----------------------------------------------------

void Opm::commit_write(uint8_t addr, uint8_t data) noexcept {
    s_.regs[addr] = data;
    (void)addr;
    (void)data;
}

// --- per-cycle engine -----------------------------------------------------

void Opm::clock() noexcept {
    if (s_.latch.busy) s_.latch.busy--;
    if (s_.latch.pending) {
        s_.latch.pending = 0;
        commit_write(s_.latch.addr, s_.latch.data);
    }
    const uint8_t cyc = s_.cycle;
    if (cyc == 0) begin_sample();
    run_slot(cyc);
    if (cyc == kCyclesPerSample - 1) {
        end_sample();
        s_.cycle = 0;
    } else {
        s_.cycle = static_cast<uint8_t>(cyc + 1);
    }
}

void Opm::begin_sample() noexcept {
    s_.mix_l = 0;
    s_.mix_r = 0;
}

void Opm::run_slot(uint32_t slot) noexcept { (void)slot; }

void Opm::end_sample() noexcept {
    s_.out_l = s_.mix_l;
    s_.out_r = s_.mix_r;
}

void Opm::update_eg_timing() noexcept {}
void Opm::update_lfo() noexcept {}
void Opm::update_noise() noexcept {}
void Opm::update_timers() noexcept {}
void Opm::recompute_phase_inc(int op) noexcept { (void)op; }

// --- rendering ------------------------------------------------------------

void Opm::generate(int32_t* left, int32_t* right, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        for (uint32_t c = 0; c < kCyclesPerSample; ++c) clock();
        if (left) left[i] = s_.out_l;
        if (right) right[i] = s_.out_r;
    }
}

static inline int16_t sat16(int32_t v) noexcept {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

void Opm::generate_interleaved(int16_t* lr, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        for (uint32_t c = 0; c < kCyclesPerSample; ++c) clock();
        lr[2 * i] = sat16(s_.out_l);
        lr[2 * i + 1] = sat16(s_.out_r);
    }
}

} // namespace nullopm
