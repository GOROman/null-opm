# API

## C++ (`nullopm/opm.hpp`)

```cpp
nullopm::Opm chip(3579545);           // clock in Hz; sample_rate() = clock / 64
chip.set_callbacks({irq_fn, ct_fn, user});
chip.write_address(0x20); chip.write_data(0xC7);   // or chip.write(a0, data)
uint8_t st = chip.read_status();      // bit7 busy, bit1 timer B, bit0 timer A
chip.clock();                         // one internal cycle (32 per sample)
chip.generate(l, r, n);               // int32 per channel, 16-bit range
chip.generate_interleaved(lr, n);     // int16 L R L R …
chip.reset();                         // IC pin
Opm saved = chip;                     // save state (plain copy)
```

Register writes take effect at the next `clock()` / sample; call `clock()` at
least once between data writes or the second write replaces the first (as on
hardware). The tools' player gives each write one cycle.

Introspection: `peek_register(addr)`, `dropped_writes()`, `state()` (read-only
view of all internal state; layout is not a stable API).

## C (`nullopm/opm_c.h`)

```c
nullopm_t* c = nullopm_create(3579545);                 /* heap */
nullopm_t* c = nullopm_create_in(buf, len, 3579545);    /* caller memory, nullopm_size() bytes */
nullopm_write_reg(c, 0x20, 0xC7);                       /* address + data, no clock needed */
nullopm_generate_i16(c, lr, n);
nullopm_set_irq_callback(c, cb, user);
nullopm_destroy(c);
```

All functions are exported from the WASM build (`EMSCRIPTEN_KEEPALIVE`).
