// SPDX-License-Identifier: MIT
#include "vgm.hpp"

#include <cstdio>
#include <cstring>

#ifdef NULLOPM_HAVE_ZLIB
#include <zlib.h>
#endif

namespace nullopm::tools {

namespace {
bool read_file(const std::string& path, std::vector<uint8_t>& out, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

bool gunzip(std::vector<uint8_t>& data, std::string* err) {
#ifdef NULLOPM_HAVE_ZLIB
    std::vector<uint8_t> out;
    z_stream zs{};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
        if (err) *err = "inflateInit2 failed";
        return false;
    }
    zs.next_in = data.data();
    zs.avail_in = static_cast<uInt>(data.size());
    uint8_t buf[65536];
    int rc;
    do {
        zs.next_out = buf;
        zs.avail_out = sizeof buf;
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&zs);
            if (err) *err = "gzip inflate failed";
            return false;
        }
        out.insert(out.end(), buf, buf + (sizeof buf - zs.avail_out));
    } while (rc != Z_STREAM_END);
    inflateEnd(&zs);
    data.swap(out);
    return true;
#else
    (void)data;
    if (err) *err = "compressed VGM (vgz) needs zlib; gunzip the file first";
    return false;
#endif
}

uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

std::string read_gd3_title(const std::vector<uint8_t>& d, size_t off) {
    if (off + 12 > d.size() || std::memcmp(&d[off], "Gd3 ", 4)) return {};
    size_t p = off + 12;
    std::string s;
    while (p + 1 < d.size()) {
        const uint16_t ch = rd16(&d[p]);
        p += 2;
        if (ch == 0) break;
        if (ch < 0x80) s.push_back(char(ch));
        else if (ch < 0x800) {
            s.push_back(char(0xC0 | (ch >> 6)));
            s.push_back(char(0x80 | (ch & 0x3F)));
        } else {
            s.push_back(char(0xE0 | (ch >> 12)));
            s.push_back(char(0x80 | ((ch >> 6) & 0x3F)));
            s.push_back(char(0x80 | (ch & 0x3F)));
        }
    }
    return s;
}
} // namespace

bool read_vgm(const std::string& path, RegStream& out, std::string* err) {
    std::vector<uint8_t> d;
    if (!read_file(path, d, err)) return false;
    if (d.size() >= 2 && d[0] == 0x1F && d[1] == 0x8B && !gunzip(d, err)) return false;
    if (d.size() < 0x40 || std::memcmp(d.data(), "Vgm ", 4)) {
        if (err) *err = "not a VGM file: " + path;
        return false;
    }
    const uint32_t version = rd32(&d[8]);
    out = RegStream{};
    out.clock_hz = version >= 0x110 ? rd32(&d[0x30]) & 0x3FFFFFFF : 0;
    out.total_time = rd32(&d[0x18]);
    const uint32_t loop_off = rd32(&d[0x1C]);
    const uint32_t loop_samples = rd32(&d[0x20]);
    size_t pos = 0x40;
    if (version >= 0x150) {
        const uint32_t data_off = rd32(&d[0x34]);
        if (data_off) pos = 0x34 + data_off;
    }
    const uint32_t gd3 = rd32(&d[0x14]);
    if (gd3) out.gd3_title = read_gd3_title(d, 0x14 + gd3);
    const size_t loop_pos = loop_off ? 0x1C + loop_off : 0;
    out.has_loop = loop_off != 0 && loop_samples != 0;

    uint64_t t = 0;
    while (pos < d.size()) {
        if (out.has_loop && pos == loop_pos) out.loop_time = t;
        const uint8_t cmd = d[pos];
        auto need = [&](size_t n) {
            if (pos + n > d.size()) {
                pos = d.size();
                return false;
            }
            return true;
        };
        if (cmd == 0x54) {
            if (!need(3)) break;
            out.events.push_back({t, d[pos + 1], d[pos + 2]});
            pos += 3;
        } else if (cmd == 0x61) {
            if (!need(3)) break;
            t += rd16(&d[pos + 1]);
            pos += 3;
        } else if (cmd == 0x62) {
            t += 735;
            pos += 1;
        } else if (cmd == 0x63) {
            t += 882;
            pos += 1;
        } else if (cmd == 0x66) {
            break;
        } else if (cmd >= 0x70 && cmd <= 0x7F) {
            t += uint64_t(cmd & 15) + 1;
            pos += 1;
        } else if (cmd >= 0x80 && cmd <= 0x8F) {
            t += cmd & 15;
            pos += 1;
        } else if (cmd == 0x67) {
            if (!need(7)) break;
            pos += 7 + rd32(&d[pos + 3]);
        } else if (cmd == 0x68) {
            pos += 12;
        } else if (cmd == 0x4F || cmd == 0x50 || (cmd >= 0x30 && cmd <= 0x3F)) {
            pos += 2;
        } else if ((cmd >= 0x51 && cmd <= 0x5F) || (cmd >= 0xA0 && cmd <= 0xBF) ||
                   (cmd >= 0x40 && cmd <= 0x4E)) {
            pos += 3;
        } else if (cmd >= 0xC0 && cmd <= 0xDF) {
            pos += 4;
        } else if (cmd >= 0xE0) {
            pos += 5;
        } else if (cmd == 0x90 || cmd == 0x91 || cmd == 0x95) {
            pos += 5;
        } else if (cmd == 0x92) {
            pos += 6;
        } else if (cmd == 0x93) {
            pos += 11;
        } else if (cmd == 0x94) {
            pos += 2;
        } else {
            if (err) *err = "unknown VGM command 0x" + std::to_string(cmd);
            return false;
        }
    }
    if (out.total_time == 0) out.total_time = t;
    return true;
}

bool read_regdump(const std::string& path, RegStream& out, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    out = RegStream{};
    char line[256];
    uint64_t t = 0;
    while (std::fgets(line, sizeof line, f)) {
        unsigned a, dd;
        unsigned long long tt;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (std::sscanf(line, "clock %u", &a) == 1) {
            out.clock_hz = a;
        } else if (std::sscanf(line, "t%llu %x %x", &tt, &a, &dd) == 3) {
            t = tt;
            out.events.push_back({t, uint8_t(a), uint8_t(dd)});
        } else if (std::sscanf(line, "w %x %x", &a, &dd) == 2) {
            out.events.push_back({t, uint8_t(a), uint8_t(dd)});
        } else if (std::sscanf(line, "wait %llu", &tt) == 1) {
            t += tt;
        } else if (std::sscanf(line, "end %llu", &tt) == 1) {
            t = tt;
        }
    }
    std::fclose(f);
    out.total_time = t;
    return true;
}

bool read_regstream(const std::string& path, RegStream& out, std::string* err) {
    const auto dot = path.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : path.substr(dot);
    if (ext == ".txt" || ext == ".regdump" || ext == ".log") return read_regdump(path, out, err);
    return read_vgm(path, out, err);
}

bool write_regdump(const std::string& path, const RegStream& s, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    std::fprintf(f, "# null-opm register log: t<time/44100> <addr> <data>\n");
    if (s.clock_hz) std::fprintf(f, "clock %u\n", s.clock_hz);
    for (const auto& e : s.events) std::fprintf(f, "t%llu %02X %02X\n", (unsigned long long)e.time, e.addr, e.data);
    std::fprintf(f, "end %llu\n", (unsigned long long)s.total_time);
    std::fclose(f);
    return true;
}

} // namespace nullopm::tools
