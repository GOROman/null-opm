// null-opm: clean-room YM2151 (OPM) emulator
// SPDX-License-Identifier: MIT
#ifndef NULLOPM_OPM_HPP
#define NULLOPM_OPM_HPP

#include "nullopm/opm_types.hpp"

namespace nullopm {

namespace detail {

struct OpState {
    // register fields
    uint8_t dt1 = 0, mul = 0, tl = 0, ks = 0, ar = 0, ame = 0, d1r = 0, dt2 = 0, d2r = 0, d1l = 0,
            rr = 0;
    // phase generator
    uint32_t phase = 0;     // 20-bit phase accumulator
    uint32_t phase_inc = 0; // computed increment (20-bit domain)
    // envelope generator
    uint16_t eg_level = 1023; // 10-bit attenuation, 0 = loudest
    uint8_t eg_state = 0;     // 0 attack, 1 decay1, 2 decay2, 3 release
    uint8_t key = 0;          // key pending state (1 = on)
    uint8_t key_on = 0;       // effective key state
    uint8_t key_csm = 0;      // CSM-triggered key pulse
    // output history (for feedback: two most recent outputs)
    int16_t out0 = 0, out1 = 0;
};

struct ChState {
    uint8_t kc = 0, kf = 0, fb = 0, con = 0, pan = 0, pms = 0, ams = 0;
    int32_t acc = 0; // carrier sum for this sample
};

struct Timers {
    uint16_t a_period = 0, a_count = 0;
    uint8_t b_period = 0, b_count = 0, b_prescale = 0;
    uint8_t ctrl = 0;   // reg 0x14
    uint8_t status = 0; // overflow flags
};

struct Lfo {
    uint8_t freq = 0, wave = 0, pmd = 0, amd = 0, reset = 0;
    uint32_t counter = 0;
    uint8_t phase = 0;
    uint32_t noise_lfsr = 1;
    uint8_t noise_val = 0;
    uint8_t am = 0;    // 0..127 (unsigned)
    int16_t pm = 0;    // signed
};

struct Noise {
    uint8_t enable = 0, freq = 0;
    uint32_t lfsr = 1;
    uint8_t counter = 0;
    uint8_t out = 0;
};

struct WriteLatch {
    uint8_t addr = 0, data = 0;
    uint8_t pending = 0;
    uint8_t busy = 0; // remaining busy cycles
    uint32_t dropped = 0;
};

struct State {
    OpState op[kOperators];
    ChState ch[kChannels];
    Timers timers;
    Lfo lfo;
    Noise noise;
    WriteLatch latch;
    uint8_t regs[256] = {};
    uint8_t address = 0;
    uint8_t ct = 0;
    uint8_t irq = 0;
    uint8_t cycle = 0;      // 0..31 within the current sample
    uint32_t eg_counter = 0;
    uint32_t clock_hz = kDefaultClockHz;
    int32_t out_l = 0, out_r = 0; // last completed sample
    int32_t mix_l = 0, mix_r = 0; // accumulating sample
    Callbacks cb;
};

} // namespace detail

/// One YM2151 chip. Copyable (copying is a save-state); no heap, no global state.
class Opm {
public:
    /// Cycles (clock/2 domain) the busy flag stays set after a data write.
    /// Datasheet: wait ~68 master clocks between writes -> 34 phi1 cycles.
    static constexpr uint8_t kBusyCycles = 34;

    explicit Opm(uint32_t clock_hz = kDefaultClockHz) noexcept;

    /// Hardware reset (IC pin). Clears all registers and internal state.
    void reset() noexcept;
    void set_callbacks(const Callbacks& cb) noexcept;

    uint32_t clock_hz() const noexcept { return s_.clock_hz; }
    /// Native output rate: clock / 64 (55930 Hz at the default clock).
    uint32_t sample_rate() const noexcept { return s_.clock_hz / kMasterClocksPerSample; }

    // --- bus interface -------------------------------------------------
    /// Generic bus write. a0 = 0 selects the address latch, a0 = 1 writes data.
    void write(uint8_t a0, uint8_t data) noexcept;
    void write_address(uint8_t addr) noexcept;
    void write_data(uint8_t data) noexcept;
    /// Status register: bit7 busy, bit1 timer B overflow, bit0 timer A overflow.
    uint8_t read_status() const noexcept;
    bool irq_asserted() const noexcept { return s_.irq != 0; }
    /// CT output pins: bit0 = CT1, bit1 = CT2.
    uint8_t ct() const noexcept { return s_.ct; }

    // --- audio ---------------------------------------------------------
    /// Advance one internal cycle (clock/2 domain). 32 cycles produce one sample.
    void clock() noexcept;
    /// Render n native-rate samples. Values are in 16-bit range stored in int32.
    void generate(int32_t* left, int32_t* right, size_t n) noexcept;
    /// Render n native-rate samples as interleaved saturated int16 (L, R, L, R ...).
    void generate_interleaved(int16_t* lr, size_t n) noexcept;

    // --- introspection (not for the audio path) --------------------------
    uint8_t peek_register(uint8_t addr) const noexcept { return s_.regs[addr]; }
    uint32_t dropped_writes() const noexcept { return s_.latch.dropped; }
    const detail::State& state() const noexcept { return s_; }

private:
    void commit_write(uint8_t addr, uint8_t data) noexcept;
    void run_slot(uint32_t slot) noexcept;
    void begin_sample() noexcept;
    void end_sample() noexcept;
    void update_eg_timing() noexcept;
    void update_lfo() noexcept;
    void update_noise() noexcept;
    void update_timers() noexcept;
    void set_irq(bool on) noexcept;
    void recompute_phase_inc(int op) noexcept;

    detail::State s_;
};

/// Library version string, e.g. "0.1.0".
const char* version() noexcept;

} // namespace nullopm

#endif // NULLOPM_OPM_HPP
