#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::savefile {
// GTA copies load filenames into two char[104] buffers (0xC15FC8 and 0xC15F60).
// Keep only these short tokens there; resolve the full path at CFileMgr::OpenFile.
inline constexpr char kLoadPrefix[] = "__sacoop_load__";
inline constexpr char kLoadBase[] = "__sacoop_load__8";  // native slot 7 is the eighth slot
inline constexpr char kLoadToken[] = "__sacoop_load__8.b";
inline constexpr char kSnapshotToken[] = "__sacoop_snapshot__.b";
static_assert(sizeof(kLoadToken) <= 104 && sizeof(kSnapshotToken) <= 104);

inline const char* Resolve(const char* path, const std::string& load, const std::string& snapshot) {
    if (path && !load.empty() && (!std::strcmp(path, kLoadToken) || !std::strcmp(path, kLoadBase))) return load.c_str();
    if (path && !snapshot.empty() && !std::strcmp(path, kSnapshotToken)) return snapshot.c_str();
    return path;
}

// Let the original game construct slot names, including its extension and register ABI.
// The replacement prefix is scoped to the native validation call, never normal save slots.
template <size_t N> class ScopedLoadPrefix {
    char (&target)[N];
    char previous[N];
public:
    explicit ScopedLoadPrefix(char (&buffer)[N]) : target(buffer) {
        static_assert(N >= sizeof(kLoadPrefix));
        std::memcpy(previous, target, N);
        std::strcpy(target, kLoadPrefix);
    }
    ~ScopedLoadPrefix() { std::memcpy(target, previous, N); }
    ScopedLoadPrefix(const ScopedLoadPrefix&) = delete;
    ScopedLoadPrefix& operator=(const ScopedLoadPrefix&) = delete;
};

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

// LoadWorkBuffer refuses an unaligned final read. Add only zero padding before the
// checksum, after all save blocks. This preserves the checksum and every serialized byte.
inline bool Align(std::vector<uint8_t>& bytes) {
    if (!Valid(bytes)) return false;
    const size_t padding = (4 - bytes.size() % 4) % 4;
    bytes.insert(bytes.end() - 4, padding, uint8_t{0});
    return true;
}

// Native validation consults C_PcSave::error even when this particular read succeeds.
// A previous missing/corrupt slot must not poison a later valid load.
template <typename Check> bool Validate(int32_t& error, Check check) {
    error = 0;
    return check();
}
}
