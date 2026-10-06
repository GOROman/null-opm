// null-opm on ESP32 with I2S output (Arduino core 3.x, ESP_I2S).
// Wiring: BCLK=GPIO26, LRCLK=GPIO25, DATA=GPIO22 (change below).
// SPDX-License-Identifier: MIT
#include <ESP_I2S.h>
#include <nullopm/opm.hpp>

static nullopm::Opm chip; // ~2 KB, static so it lives outside the task stack
static I2SClass i2s;
static int16_t buffer[2 * 256];

static void w(uint8_t a, uint8_t d) {
    chip.write_address(a);
    chip.write_data(d);
    chip.clock();
}

void setup() {
    i2s.setPins(26, 25, 22);
    i2s.begin(I2S_MODE_STD, chip.sample_rate(), I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    // electric-piano style patch on channel 0
    const uint8_t patch[][2] = {
        {0x20, 0xC5}, {0x28, 0x4A}, {0x40, 0x31}, {0x48, 0x02}, {0x50, 0x01}, {0x58, 0x01},
        {0x60, 0x20}, {0x68, 0x18}, {0x70, 0x00}, {0x78, 0x00}, {0x80, 0x1F}, {0x88, 0x1F},
        {0x90, 0x1F}, {0x98, 0x1F}, {0xA0, 0x0A}, {0xA8, 0x08}, {0xB0, 0x06}, {0xB8, 0x06},
        {0xE0, 0x2F}, {0xE8, 0x2F}, {0xF0, 0x3F}, {0xF8, 0x3F}};
    for (auto& p : patch) w(p[0], p[1]);
}

void loop() {
    static uint32_t frames = 0;
    if (frames % (chip.sample_rate() / 2) == 0) w(0x08, 0x78); // key on twice a second
    if (frames % (chip.sample_rate() / 2) == chip.sample_rate() / 4) w(0x08, 0x00);
    chip.generate_interleaved(buffer, 256);
    i2s.write(reinterpret_cast<uint8_t*>(buffer), sizeof buffer);
    frames += 256;
}
