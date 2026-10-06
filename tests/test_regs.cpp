// Register / bus semantics and multi-instance independence.
#include "doctest.h"
#include "nullopm/opm.hpp"
#include "nullopm/opm_c.h"

#include <cstring>
#include <vector>

using namespace nullopm;

TEST_CASE("silent after reset") {
    Opm chip;
    std::vector<int32_t> l(256), r(256);
    chip.generate(l.data(), r.data(), l.size());
    for (size_t i = 0; i < l.size(); ++i) {
        CHECK(l[i] == 0);
        CHECK(r[i] == 0);
    }
    CHECK(chip.sample_rate() == 55930);
}

TEST_CASE("write latch commits on next cycle and raises busy") {
    Opm chip;
    chip.write_address(0x20);
    chip.write_data(0xC7);
    CHECK((chip.read_status() & status::kBusy) != 0);
    CHECK(chip.peek_register(0x20) == 0);
    chip.clock();
    CHECK(chip.peek_register(0x20) == 0xC7);
    // busy clears after the documented number of cycles
    int cycles = 1;
    while (chip.read_status() & status::kBusy) {
        chip.clock();
        ++cycles;
        REQUIRE(cycles < 1000);
    }
    CHECK(cycles == Opm::kBusyCycles);
}

TEST_CASE("write while busy replaces the pending write") {
    Opm chip;
    chip.write_address(0x20);
    chip.write_data(0x11);
    chip.write_data(0x22);
    CHECK(chip.dropped_writes() == 1);
    chip.clock();
    CHECK(chip.peek_register(0x20) == 0x22);
}

TEST_CASE("instances are independent") {
    Opm a, b;
    a.write_address(0x28);
    a.write_data(0x4A);
    a.clock();
    CHECK(a.peek_register(0x28) == 0x4A);
    CHECK(b.peek_register(0x28) == 0x00);
    Opm c = a; // copy = save state
    CHECK(c.peek_register(0x28) == 0x4A);
}

TEST_CASE("C API placement construction") {
    alignas(16) static unsigned char buf[8192];
    REQUIRE(nullopm_size() <= sizeof(buf));
    nullopm_t* chip = nullopm_create_in(buf, sizeof(buf), kDefaultClockHz);
    REQUIRE(chip != nullptr);
    CHECK(nullopm_sample_rate(chip) == 55930);
    nullopm_write_reg(chip, 0x20, 0xC0);
    int16_t lr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    nullopm_generate_i16(chip, lr, 4);
    for (int16_t v : lr) CHECK(v == 0);
    CHECK(std::strcmp(nullopm_version(), version()) == 0);
    nullopm_t* heap = nullopm_create(kDefaultClockHz);
    REQUIRE(heap != nullptr);
    nullopm_destroy(heap);
}
