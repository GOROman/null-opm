// null-opm on ESP-IDF 5.x with the std I2S driver.  SPDX-License-Identifier: MIT
#include "driver/i2s_std.h"
#include "nullopm/opm.hpp"

static nullopm::Opm chip;
static int16_t buffer[2 * 256];

static void w(uint8_t a, uint8_t d) {
    chip.write_address(a);
    chip.write_data(d);
    chip.clock();
}

extern "C" void app_main() {
    i2s_chan_handle_t tx;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx, nullptr);
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(chip.sample_rate()),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = GPIO_NUM_26, .ws = GPIO_NUM_25,
                     .dout = GPIO_NUM_22, .din = I2S_GPIO_UNUSED, .invert_flags = {}},
    };
    i2s_channel_init_std_mode(tx, &std_cfg);
    i2s_channel_enable(tx);
    const uint8_t patch[][2] = {{0x20, 0xC7}, {0x28, 0x4A}, {0x58, 0x01}, {0x78, 0x00},
                                {0x98, 0x1F}, {0xB8, 0x08}, {0xF8, 0x2F}, {0x08, 0x40}};
    for (auto& p : patch) w(p[0], p[1]);
    while (true) {
        chip.generate_interleaved(buffer, 256);
        size_t written = 0;
        i2s_channel_write(tx, buffer, sizeof buffer, &written, portMAX_DELAY);
    }
}
