// null-opm: clean-room YM2151 (OPM) emulator core.
// SPDX-License-Identifier: MIT
//
// Clean-room notice: this file is written from the YM2151 / YM3012 datasheets,
// the YM2151 application manual and general FM synthesis knowledge only.
// No emulator source code was consulted. See docs/CLEANROOM.md.
#include "nullopm/opm.hpp"
#include "opm_tables.hpp"

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

// Operator index: slot-major so that op = slot * 8 + ch, matching the register map.
static inline int op_index(int slot, int ch) noexcept { return slot * 8 + ch; }

void Opm::commit_write(uint8_t addr, uint8_t data) noexcept {
    s_.regs[addr] = data;
    if (addr >= 0x40) {
        const int ch = addr & 7;
        const int slot = (addr >> 3) & 3;
        detail::OpState& o = s_.op[op_index(slot, ch)];
        switch (addr & 0xE0) {
        case reg::kDt1Mul:
            o.dt1 = (data >> 4) & 7;
            o.mul = data & 15;
            recompute_phase_inc(op_index(slot, ch));
            break;
        case reg::kTl: o.tl = data & 0x7F; break;
        case reg::kKsAr:
            o.ks = data >> 6;
            o.ar = data & 0x1F;
            break;
        case reg::kAmeD1r:
            o.ame = data >> 7;
            o.d1r = data & 0x1F;
            break;
        case reg::kDt2D2r:
            o.dt2 = data >> 6;
            o.d2r = data & 0x1F;
            recompute_phase_inc(op_index(slot, ch));
            break;
        case reg::kD1lRr:
            o.d1l = data >> 4;
            o.rr = data & 15;
            break;
        default: break;
        }
        return;
    }
    if (addr >= 0x20) {
        const int ch = addr & 7;
        detail::ChState& c = s_.ch[ch];
        switch (addr & 0xF8) {
        case reg::kChanCtrl:
            c.pan = data >> 6;
            c.fb = (data >> 3) & 7;
            c.con = data & 7;
            break;
        case reg::kKeyCode:
            c.kc = data & 0x7F;
            for (int slot = 0; slot < 4; ++slot) recompute_phase_inc(op_index(slot, ch));
            break;
        case reg::kKeyFrac:
            c.kf = data >> 2;
            for (int slot = 0; slot < 4; ++slot) recompute_phase_inc(op_index(slot, ch));
            break;
        case reg::kPmsAms:
            c.pms = (data >> 4) & 7;
            c.ams = data & 3;
            break;
        default: break;
        }
        return;
    }
    switch (addr) {
    case reg::kKeyOn: {
        const int ch = data & 7;
        for (int slot = 0; slot < 4; ++slot) {
            detail::OpState& o = s_.op[op_index(slot, ch)];
            o.key = (data >> (3 + slot)) & 1;
        }
        break;
    }
    default: break;
    }
}

// --- phase generator --------------------------------------------------------

// Key code note field (4 bits) -> semitone 0..12. Codes 3, 7, 11, 15 duplicate
// the following code (datasheet KC table lists 12 notes over 16 codes).
static inline uint32_t kc_semitone(uint8_t kc) noexcept {
    const uint32_t n = kc & 15;
    return n - (n >> 2);
}

void Opm::recompute_phase_inc(int op) noexcept {
    detail::OpState& o = s_.op[op];
    const detail::ChState& c = s_.ch[op & 7];
    // Pitch index in 1/64 semitone units: octave*768 + semitone*64 + KF + DT2 + PM.
    int32_t idx = static_cast<int32_t>(((c.kc >> 4) & 7) * 768 + kc_semitone(c.kc) * 64 + c.kf);
    idx += tables::kDt2[o.dt2];
    idx += s_.lfo.pm_applied[op & 7];
    if (idx < 0) idx = 0;
    const uint32_t oct = static_cast<uint32_t>(idx) / 768;
    const uint32_t frac = static_cast<uint32_t>(idx) % 768;
    uint32_t inc = (tables::kFnum.v[frac] << oct) >> tables::kFnumFracBits;
    // DT1 uses the 5-bit key code (KC >> 2); DT1 values 4..7 are negative 0..3.
    const uint32_t keycode = (c.kc >> 2) & 31;
    const uint32_t dt = tables::kDt1[o.dt1 & 3][keycode];
    if (o.dt1 & 4) inc -= dt;
    else inc += dt;
    // MUL: 0 = x0.5, 1..15 = x1..x15
    inc = o.mul ? inc * o.mul : inc >> 1;
    o.phase_inc = inc & 0xFFFFF;
}

// --- operator ---------------------------------------------------------------

// Sine lookup with attenuation: phase index is 10 bits (one full cycle), att is
// in 1/256 log2 units (12 bits + headroom). Returns a 14-bit signed sample.
static inline int32_t op_output(uint32_t phase10, uint32_t att) noexcept {
    uint32_t q = phase10 & 0xFF;
    if (phase10 & 0x100) q ^= 0xFF;
    uint32_t a = att + tables::kLogSin.v[q];
    if (a > 0x1FFF) a = 0x1FFF;
    const uint32_t m = (tables::kExp.v[(a & 0xFF) ^ 0xFF] | 0x400u) << 2;
    const int32_t v = static_cast<int32_t>(m >> (a >> 8));
    return (phase10 & 0x200) ? -v : v;
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

void Opm::run_slot(uint32_t slot) noexcept {
    detail::OpState& o = s_.op[slot];
    detail::ChState& c = s_.ch[slot & 7];
    // M1 milestone: envelope is a gate (0 attenuation while key is on).
    if (o.key != o.key_on) {
        o.key_on = o.key;
        if (o.key_on) {
            o.phase = 0;
            o.eg_level = 0;
        } else {
            o.eg_level = 1023;
        }
    }
    uint32_t att = o.eg_level + (static_cast<uint32_t>(o.tl) << 3);
    if (att > 1023) att = 1023;
    const int32_t out = op_output(o.phase >> 10, att << 2);
    o.phase = (o.phase + o.phase_inc) & 0xFFFFF;
    o.out1 = o.out0;
    o.out0 = static_cast<int16_t>(out);
    // M1: every operator is treated as a carrier.
    c.acc += out;
    if ((slot >> 3) == 3) { // last slot of this channel for this sample
        if (c.pan & 1) s_.mix_l += c.acc;
        if (c.pan & 2) s_.mix_r += c.acc;
        c.acc = 0;
    }
}

void Opm::end_sample() noexcept {
    s_.out_l = s_.mix_l;
    s_.out_r = s_.mix_r;
}

void Opm::update_eg_timing() noexcept {}
void Opm::update_lfo() noexcept {}
void Opm::update_noise() noexcept {}
void Opm::update_timers() noexcept {}

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
