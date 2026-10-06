// VGM / register-log reader for the null-opm tools.  SPDX-License-Identifier: MIT
#ifndef NULLOPM_TOOLS_VGM_HPP
#define NULLOPM_TOOLS_VGM_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nullopm::tools {

/// One event in a chip-agnostic, time-stamped register stream.
struct RegEvent {
    uint64_t time;    // in VGM units (1/44100 s) from the start
    uint8_t addr;
    uint8_t data;
};

struct RegStream {
    uint32_t clock_hz = 0;      // 0 = unknown (use default)
    uint64_t total_time = 0;    // 1/44100 s
    uint64_t loop_time = 0;     // 1/44100 s, time at which the loop starts (if loop)
    bool has_loop = false;
    std::vector<RegEvent> events;
    std::string gd3_title;
};

/// Parses a .vgm (or gzip-compressed .vgz when zlib is available).
bool read_vgm(const std::string& path, RegStream& out, std::string* err = nullptr);
/// Parses a text register log: lines "t<time44100> <addr hex> <data hex>" or "w <addr> <data>".
bool read_regdump(const std::string& path, RegStream& out, std::string* err = nullptr);
/// Picks a reader by extension.
bool read_regstream(const std::string& path, RegStream& out, std::string* err = nullptr);
/// Writes a RegStream as a text register log.
bool write_regdump(const std::string& path, const RegStream& s, std::string* err = nullptr);

} // namespace nullopm::tools
#endif
