#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::savefile {
// GTA copies load filenames into two char[104] buffers (0xC15FC8 and 0xC15F60).
// Keep only these short tokens there; resolve the full path at CFileMgr::OpenFile.
inline constexpr char kLoadToken[] = "__sacoop_load__.b";
inline constexpr char kSnapshotToken[] = "__sacoop_snapshot__.b";
static_assert(sizeof(kLoadToken) <= 104 && sizeof(kSnapshotToken) <= 104);

inline const char* Resolve(const char* path, const std::string& load, const std::string& snapshot) {
    if (path && !load.empty() && !std::strcmp(path, kLoadToken)) return load.c_str();
    if (path && !snapshot.empty() && !std::strcmp(path, kSnapshotToken)) return snapshot.c_str();
    return path;
}

// CheckDataNotCorrupt sums every byte except the final little-endian uint32 checksum.
// Do this before touching GTA's load state, and before publishing a host snapshot.
inline bool Valid(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 9 || std::memcmp(bytes.data(), "BLOCK", 5)) return false;
    const size_t end = bytes.size() - 4;
    uint32_t sum = 0;
    for (size_t i = 0; i < end; ++i) sum += bytes[i];
    const uint32_t stored = uint32_t(bytes[end]) | (uint32_t(bytes[end + 1]) << 8) |
                            (uint32_t(bytes[end + 2]) << 16) | (uint32_t(bytes[end + 3]) << 24);
    return sum == stored;
}

// Native validation consults C_PcSave::error even when this particular read succeeds.
// A previous missing/corrupt slot must not poison a later valid load.
template <typename Check> bool Validate(int32_t& error, Check check) {
    error = 0;
    return check();
}
}
