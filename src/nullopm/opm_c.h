/* null-opm: C API for the YM2151 (OPM) emulator.  SPDX-License-Identifier: MIT */
#ifndef NULLOPM_OPM_C_H
#define NULLOPM_OPM_C_H

#include <stddef.h>
#include <stdint.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define NULLOPM_API EMSCRIPTEN_KEEPALIVE
#else
#define NULLOPM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nullopm_t nullopm_t;
typedef void (*nullopm_irq_cb)(void* user, int asserted);
typedef void (*nullopm_ct_cb)(void* user, uint8_t ct_bits);

/* Size in bytes required by nullopm_create_in(). */
NULLOPM_API size_t nullopm_size(void);
/* Heap-allocating constructor (host / WASM). Returns NULL on failure. */
NULLOPM_API nullopm_t* nullopm_create(uint32_t clock_hz);
/* Placement constructor for MCUs: buf must be at least nullopm_size() bytes, 8-byte aligned. */
NULLOPM_API nullopm_t* nullopm_create_in(void* buf, size_t len, uint32_t clock_hz);
/* Destroys a chip made by nullopm_create(). For create_in() this only runs the destructor. */
NULLOPM_API void nullopm_destroy(nullopm_t* chip);

NULLOPM_API void nullopm_reset(nullopm_t* chip);
NULLOPM_API uint32_t nullopm_sample_rate(const nullopm_t* chip);
NULLOPM_API void nullopm_write(nullopm_t* chip, uint8_t a0, uint8_t data);
NULLOPM_API void nullopm_write_reg(nullopm_t* chip, uint8_t addr, uint8_t data);
NULLOPM_API uint8_t nullopm_read_status(const nullopm_t* chip);
NULLOPM_API void nullopm_clock(nullopm_t* chip);
NULLOPM_API void nullopm_generate(nullopm_t* chip, int32_t* left, int32_t* right, size_t n);
NULLOPM_API void nullopm_generate_i16(nullopm_t* chip, int16_t* lr, size_t n);
NULLOPM_API void nullopm_set_irq_callback(nullopm_t* chip, nullopm_irq_cb cb, void* user);
NULLOPM_API void nullopm_set_ct_callback(nullopm_t* chip, nullopm_ct_cb cb, void* user);
NULLOPM_API const char* nullopm_version(void);

#ifdef __cplusplus
}
#endif
#endif /* NULLOPM_OPM_C_H */
