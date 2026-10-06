// Drives an Opm from a RegStream with cycle-accurate timing.  SPDX-License-Identifier: MIT
#ifndef NULLOPM_TOOLS_PLAYER_HPP
#define NULLOPM_TOOLS_PLAYER_HPP

#include "nullopm/opm.hpp"
#include "vgm.hpp"

#include <vector>

namespace nullopm::tools {

struct RenderOptions {
    uint32_t clock_hz = 0;   // 0 = from stream, else default
    int loops = 1;           // times the loop section is played (if the stream loops)
    double max_seconds = 0;  // 0 = whole stream
    double tail_seconds = 0; // extra silence rendered after the end
};

/// Renders the stream to interleaved int16 stereo at the chip's native rate.
/// Register writes consume one internal cycle each, like a real CPU would.
std::vector<int16_t> render_stream(const RegStream& s, const RenderOptions& opt,
                                   uint32_t* out_rate, uint32_t* dropped_writes = nullptr);

} // namespace nullopm::tools
#endif
