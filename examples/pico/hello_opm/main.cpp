// null-opm on RP2040: 8-bit PWM audio on GPIO 0 at the chip's native rate.
// SPDX-License-Identifier: MIT
#include "hardware/pwm.h"
#include "nullopm/opm.hpp"
#include "pico/stdlib.h"

static nullopm::Opm chip;

static void w(uint8_t a, uint8_t d) {
    chip.write_address(a);
    chip.write_data(d);
    chip.clock();
}

int main() {
    gpio_set_function(0, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(0);
    pwm_set_wrap(slice, 255);
    pwm_set_enabled(slice, true);
    const uint8_t patch[][2] = {{0x20, 0xC7}, {0x28, 0x4A}, {0x58, 0x01}, {0x78, 0x00},
                                {0x98, 0x1F}, {0xB8, 0x08}, {0xF8, 0x2F}, {0x08, 0x40}};
    for (auto& p : patch) w(p[0], p[1]);
    const uint32_t period_us = 1000000u / chip.sample_rate();
    absolute_time_t next = get_absolute_time();
    while (true) {
        int32_t l, r;
        chip.generate(&l, &r, 1);
        pwm_set_gpio_level(0, static_cast<uint16_t>((l >> 8) + 128));
        next = delayed_by_us(next, period_us);
        sleep_until(next);
    }
}
