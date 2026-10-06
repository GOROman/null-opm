// null-opm: clean-room YM2151 (OPM) emulator
// SPDX-License-Identifier: MIT
#ifndef NULLOPM_OPM_TYPES_HPP
#define NULLOPM_OPM_TYPES_HPP

#include <cstddef>
#include <cstdint>

namespace nullopm {

/// Default master clock (3.579545 MHz, the common NTSC colour-burst derived clock).
constexpr uint32_t kDefaultClockHz = 3579545;

/// Internal (phi1 = clock/2) cycles per output sample: one per operator slot.
constexpr uint32_t kCyclesPerSample = 32;

/// Master clock cycles per output sample.
constexpr uint32_t kMasterClocksPerSample = kCyclesPerSample * 2;

constexpr int kChannels = 8;
constexpr int kOperatorsPerChannel = 4;
constexpr int kOperators = kChannels * kOperatorsPerChannel;

/// Register addresses (the chip has a single 8-bit address space, 0x00..0xFF).
namespace reg {
constexpr uint8_t kTest = 0x01;       // bit1: LFO reset
constexpr uint8_t kKeyOn = 0x08;      // bits6..3: C2 C1 M2 M1 (slot key bits), bits2..0: channel
constexpr uint8_t kNoise = 0x0F;      // bit7: NE, bits4..0: NFRQ
constexpr uint8_t kTimerAHi = 0x10;   // Timer A bits 9..2
constexpr uint8_t kTimerALo = 0x11;   // Timer A bits 1..0
constexpr uint8_t kTimerB = 0x12;     // Timer B 8 bits
constexpr uint8_t kTimerCtrl = 0x14;  // CSM, -, FRESET B/A, IRQEN B/A, LOAD B/A
constexpr uint8_t kLfoFreq = 0x18;    // LFRQ
constexpr uint8_t kLfoDepth = 0x19;   // bit7: 0=AMD 1=PMD, bits6..0: depth
constexpr uint8_t kCtLfoWave = 0x1B;  // bit7: CT2, bit6: CT1, bits1..0: LFO waveform
constexpr uint8_t kChanCtrl = 0x20;   // +ch: R L FB(3) CON(3)
constexpr uint8_t kKeyCode = 0x28;    // +ch: KC
constexpr uint8_t kKeyFrac = 0x30;    // +ch: KF (bits7..2)
constexpr uint8_t kPmsAms = 0x38;     // +ch: PMS(bits6..4) AMS(bits1..0)
constexpr uint8_t kDt1Mul = 0x40;     // +slot*8+ch: DT1(bits6..4) MUL(bits3..0)
constexpr uint8_t kTl = 0x60;         // +slot*8+ch: TL (7 bits)
constexpr uint8_t kKsAr = 0x80;       // +slot*8+ch: KS(bits7..6) AR(bits4..0)
constexpr uint8_t kAmeD1r = 0xA0;     // +slot*8+ch: AMS-EN(bit7) D1R(bits4..0)
constexpr uint8_t kDt2D2r = 0xC0;     // +slot*8+ch: DT2(bits7..6) D2R(bits3..0)
constexpr uint8_t kD1lRr = 0xE0;      // +slot*8+ch: D1L(bits7..4) RR(bits3..0)
} // namespace reg

/// Status register bits.
namespace status {
constexpr uint8_t kTimerA = 0x01;
constexpr uint8_t kTimerB = 0x02;
constexpr uint8_t kBusy = 0x80;
} // namespace status

/// Operator slot index used in register addressing (0x40 + slot*8 + ch) and key-on bits.
enum class Slot : uint8_t { M1 = 0, M2 = 1, C1 = 2, C2 = 3 };

/// Host callbacks. Plain function pointers so the core stays allocation-free.
struct Callbacks {
    /// Called when the IRQ pin changes state.
    void (*irq)(void* user, bool asserted) = nullptr;
    /// Called when CT1/CT2 change. bit0 = CT1, bit1 = CT2.
    void (*ct)(void* user, uint8_t ct_bits) = nullptr;
    void* user = nullptr;
};

} // namespace nullopm

#endif // NULLOPM_OPM_TYPES_HPP
