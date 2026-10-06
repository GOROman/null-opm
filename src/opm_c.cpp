// null-opm: C API implementation.  SPDX-License-Identifier: MIT
#include "nullopm/opm_c.h"
#include "nullopm/opm.hpp"

#include <new>

namespace {
struct CChip {
    nullopm::Opm opm;
    nullopm_irq_cb irq_cb = nullptr;
    void* irq_user = nullptr;
    nullopm_ct_cb ct_cb = nullptr;
    void* ct_user = nullptr;
    explicit CChip(uint32_t clk) : opm(clk) {}
};

void irq_tramp(void* user, bool on) {
    auto* c = static_cast<CChip*>(user);
    if (c->irq_cb) c->irq_cb(c->irq_user, on ? 1 : 0);
}
void ct_tramp(void* user, uint8_t bits) {
    auto* c = static_cast<CChip*>(user);
    if (c->ct_cb) c->ct_cb(c->ct_user, bits);
}
void install(CChip* c) {
    nullopm::Callbacks cb;
    cb.irq = irq_tramp;
    cb.ct = ct_tramp;
    cb.user = c;
    c->opm.set_callbacks(cb);
}
inline CChip* C(nullopm_t* p) { return reinterpret_cast<CChip*>(p); }
inline const CChip* C(const nullopm_t* p) { return reinterpret_cast<const CChip*>(p); }
} // namespace

extern "C" {

size_t nullopm_size(void) { return sizeof(CChip); }

nullopm_t* nullopm_create(uint32_t clock_hz) {
    auto* c = new (std::nothrow) CChip(clock_hz);
    if (!c) return nullptr;
    install(c);
    return reinterpret_cast<nullopm_t*>(c);
}

nullopm_t* nullopm_create_in(void* buf, size_t len, uint32_t clock_hz) {
    if (!buf || len < sizeof(CChip)) return nullptr;
    if (reinterpret_cast<uintptr_t>(buf) % alignof(CChip) != 0) return nullptr;
    auto* c = new (buf) CChip(clock_hz);
    install(c);
    return reinterpret_cast<nullopm_t*>(c);
}

void nullopm_destroy(nullopm_t* chip) {
    if (chip) delete C(chip);
}

void nullopm_reset(nullopm_t* chip) { C(chip)->opm.reset(); }
uint32_t nullopm_sample_rate(const nullopm_t* chip) { return C(chip)->opm.sample_rate(); }
void nullopm_write(nullopm_t* chip, uint8_t a0, uint8_t data) { C(chip)->opm.write(a0, data); }
void nullopm_write_reg(nullopm_t* chip, uint8_t addr, uint8_t data) {
    C(chip)->opm.write_address(addr);
    C(chip)->opm.write_data(data);
}
uint8_t nullopm_read_status(const nullopm_t* chip) { return C(chip)->opm.read_status(); }
void nullopm_clock(nullopm_t* chip) { C(chip)->opm.clock(); }
void nullopm_generate(nullopm_t* chip, int32_t* l, int32_t* r, size_t n) {
    C(chip)->opm.generate(l, r, n);
}
void nullopm_generate_i16(nullopm_t* chip, int16_t* lr, size_t n) {
    C(chip)->opm.generate_interleaved(lr, n);
}
void nullopm_set_irq_callback(nullopm_t* chip, nullopm_irq_cb cb, void* user) {
    C(chip)->irq_cb = cb;
    C(chip)->irq_user = user;
}
void nullopm_set_ct_callback(nullopm_t* chip, nullopm_ct_cb cb, void* user) {
    C(chip)->ct_cb = cb;
    C(chip)->ct_user = user;
}
const char* nullopm_version(void) { return nullopm::version(); }

} // extern "C"
