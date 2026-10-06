// Timers A/B, IRQ, status flags, CSM and reset.
#include "doctest.h"
#include "nullopm/opm.hpp"

#include <vector>

using namespace nullopm;

namespace {
void w(Opm& c, uint8_t a, uint8_t d) {
    c.write_address(a);
    c.write_data(d);
    c.clock();
}
// Samples until a status bit appears (counted from the next sample boundary).
size_t samples_until_flag(Opm& c, uint8_t bit, size_t limit) {
    // A register write made at cycle 0 already started a sample: finish it and count it.
    const size_t partial = c.state().cycle != 0 ? 1 : 0;
    while (c.state().cycle != 0) c.clock();
    if (partial && (c.read_status() & bit)) return 1;
    int32_t l, r;
    for (size_t i = 0; i < limit; ++i) {
        c.generate(&l, &r, 1);
        if (c.read_status() & bit) return i + 1 + partial;
    }
    return 0;
}
} // namespace

TEST_CASE("Timer A period is 1024 - N samples") {
    for (uint16_t n : {0u, 512u, 1000u, 1023u}) {
        Opm c;
        w(c, 0x10, uint8_t(n >> 2));
        w(c, 0x11, uint8_t(n & 3));
        while (c.state().cycle != 0) c.clock();
        w(c, 0x14, 0x05); // load A, IRQ enable A
        CHECK(samples_until_flag(c, status::kTimerA, 2048) == 1024 - n);
    }
}

TEST_CASE("Timer B period is 16 * (256 - N) samples") {
    for (uint8_t n : {0, 200, 255}) {
        Opm c;
        w(c, 0x12, n);
        while (c.state().cycle != 0) c.clock();
        w(c, 0x14, 0x0A); // load B, IRQ enable B
        CHECK(samples_until_flag(c, status::kTimerB, 8192) == 16u * (256 - n));
    }
}

TEST_CASE("no flag without IRQ enable; reset bits clear flags; IRQ pin and callback") {
    Opm c;
    static int irq_calls;
    static bool irq_state;
    irq_calls = 0;
    irq_state = false;
    Callbacks cb;
    cb.irq = [](void*, bool on) {
        irq_state = on;
        ++irq_calls;
    };
    c.set_callbacks(cb);
    w(c, 0x10, 0xFF);
    w(c, 0x11, 0x03); // N = 1023 -> overflow every sample
    w(c, 0x14, 0x01); // load, no IRQ enable
    int32_t l[8], r[8];
    c.generate(l, r, 8);
    CHECK((c.read_status() & status::kTimerA) == 0);
    CHECK_FALSE(c.irq_asserted());
    w(c, 0x14, 0x05);
    c.generate(l, r, 4);
    CHECK((c.read_status() & status::kTimerA) != 0);
    CHECK(c.irq_asserted());
    CHECK(irq_calls == 1);
    CHECK(irq_state);
    w(c, 0x14, 0x14); // reset flag A, keep running with IRQ enabled: re-triggers next sample
    CHECK((c.read_status() & status::kTimerA) == 0);
    CHECK_FALSE(c.irq_asserted());
    CHECK(irq_calls == 2);
    w(c, 0x14, 0x10); // stop timer and clear
    c.generate(l, r, 8);
    CHECK((c.read_status() & status::kTimerA) == 0);
}

TEST_CASE("CSM keys all channels on Timer A overflow") {
    Opm c;
    w(c, 0x20, 0xC7);
    w(c, 0x28, 0x4A);
    for (int slot = 0; slot < 4; ++slot) {
        const uint8_t o = uint8_t(slot * 8);
        w(c, uint8_t(0x40 + o), 0x01);
        w(c, uint8_t(0x60 + o), slot == 3 ? 0x00 : 0x7F);
        w(c, uint8_t(0x80 + o), 0x1F);
        w(c, uint8_t(0xE0 + o), 0x0F); // RR 15: fast release after the pulse
    }
    w(c, 0x10, 0xF0);
    w(c, 0x11, 0x00); // N = 960 -> 64 samples
    w(c, 0x14, 0x81); // CSM + load A (no key-on register write)
    std::vector<int32_t> l(1024), r(1024);
    c.generate(l.data(), r.data(), l.size());
    size_t nonzero = 0;
    for (int32_t v : l) nonzero += v != 0;
    CHECK(nonzero > 0);
    CHECK(nonzero < l.size()); // pulses, not a sustained note
}

TEST_CASE("reset clears registers, timers and output but keeps clock and callbacks") {
    Opm c(4000000);
    static int calls;
    calls = 0;
    Callbacks cb;
    cb.ct = [](void*, uint8_t) { ++calls; };
    c.set_callbacks(cb);
    w(c, 0x1B, 0xC0);
    w(c, 0x14, 0x05);
    c.reset();
    CHECK(c.peek_register(0x1B) == 0);
    CHECK(c.ct() == 0);
    CHECK((c.read_status() & 0x83) == 0);
    CHECK(c.clock_hz() == 4000000);
    w(c, 0x1B, 0x40);
    CHECK(calls == 2);
}
