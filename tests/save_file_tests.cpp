#include <cassert>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include "shared/game/SaveFile.h"

namespace fs = std::filesystem;
using namespace coop::savefile;

std::vector<uint8_t> SaveBytes() {
    std::vector<uint8_t> bytes(202748, 0);
    std::memcpy(bytes.data(), "BLOCK", 5);
    for (size_t i = 5; i < bytes.size() - 4; ++i) bytes[i] = static_cast<uint8_t>(i);
    uint32_t checksum = 0;
    for (size_t i = 0; i < bytes.size() - 4; ++i) checksum += bytes[i];
    for (unsigned i = 0; i < 4; ++i) bytes[bytes.size() - 4 + i] = static_cast<uint8_t>(checksum >> (8 * i));
    return bytes;
}

int main(int argc, char** argv) {
    assert(argc == 2);  // an isolated temporary directory, provided by the test runner
    const auto original = SaveBytes();
    assert(Valid(original));
    // Runtime snapshots may end at any byte offset; GTA reads the final block in DWORDs.
    for (int padding = 0; padding < 4; ++padding) {
        auto bytes = original;
        bytes.insert(bytes.end() - 4, padding, uint8_t{0});
        const auto before = bytes;
        assert(Valid(bytes) && Align(bytes));
        assert(bytes.size() % 4 == 0 && bytes.size() >= before.size() && Valid(bytes));
        assert(std::equal(before.begin(), before.end() - 4, bytes.begin()));
        assert(std::equal(before.end() - 4, before.end(), bytes.end() - 4));
        const auto aligned = bytes;
        assert(Align(bytes) && bytes == aligned);
    }
    assert(!Valid({}) && !Valid({'B', 'L', 'O', 'C', 'K'}));
    auto corrupted = original;
    corrupted[1000] ^= 1;
    assert(!Valid(corrupted));
    const auto badBefore = corrupted;
    assert(!Align(corrupted) && corrupted == badBefore);
    for (int removed = 1; removed <= 8; ++removed) {
        auto truncated = original;
        truncated.resize(truncated.size() - removed);
        assert(!Valid(truncated));
    }

    // The post-prologue world is under a nested per-player profile. It must pass through
    // BOTH of GTA's 104-byte buffers without copying the full pathname into either one.
    const fs::path root = fs::path(argv[1]) / std::string(60, 'a') / std::string(50, 'b');
    fs::create_directories(root);
    const std::string snapshot = (root / "host_snapshot.b").string();
    const std::string load = (root / "partner_world.b").string();
    assert(load.size() > 104);
    { std::ofstream out(Resolve(kSnapshotToken, load, snapshot), std::ios::binary);
      out.write(reinterpret_cast<const char*>(original.data()), original.size()); }
    fs::copy_file(snapshot, load);
    struct Buffers {
        char name[104]{};
        std::array<uint8_t, 16> guard;
        char withPath[104]{};
    } buffers;
    buffers.guard.fill(0xA5);
    std::strcpy(buffers.name, kLoadToken);
    std::strcpy(buffers.withPath, kLoadToken);
    for (uint8_t value : buffers.guard) assert(value == 0xA5);
    std::ifstream in(Resolve(buffers.name, load, snapshot), std::ios::binary);
    const std::vector<uint8_t> received((std::istreambuf_iterator<char>(in)), {});
    assert(received == original && Valid(received));
    assert(std::string(Resolve(buffers.withPath, load, snapshot)) == load);

    // No ABI-dependent filename detour: native naming gets a temporary short prefix,
    // with or without the extension at this stage. Restore every byte even on failure.
    char prefix[260]{};
    std::strcpy(prefix, "C:\\Users\\original\\GTASAsf");
    std::array<char, 260> backup{};
    std::memcpy(backup.data(), prefix, sizeof(prefix));
    auto validateSlot = [&] {
        ScopedLoadPrefix scoped(prefix);
        const auto base = std::string(prefix) + "8";
        assert(base == kLoadBase);
        assert(std::string(Resolve(base.c_str(), load, snapshot)) == load);
        assert(std::string(Resolve((base + ".b").c_str(), load, snapshot)) == load);
        return false;
    };
    assert(!validateSlot());
    assert(!std::memcmp(prefix, backup.data(), sizeof(prefix)));
    const char otherSlot[] = "__sacoop_load__7.b";
    assert(Resolve(otherSlot, load, snapshot) == otherSlot);

    // CGenericGameStorage::CheckDataNotCorrupt also checks the sticky C_PcSave error.
    // Model a failed prior slot, a corrupt incoming save, then a valid retry.
    int32_t error = 7;
    auto check = [&] { return error == 0 && Valid(received); };
    assert(!check());
    assert(Validate(error, check));
    assert(!Validate(error, [&] { if (!Valid(corrupted)) error = 7; return error == 0; }));
    assert(Validate(error, check));
    const char normal[] = "data/script/main.scm";
    assert(Resolve(normal, load, snapshot) == normal);
    assert(Resolve(nullptr, load, snapshot) == nullptr);
    assert(Resolve(kLoadToken, {}, snapshot) == kLoadToken);
    assert(Resolve(kSnapshotToken, load, {}) == kSnapshotToken);
    assert(!fs::exists(fs::path(argv[1]) / kLoadToken));
    assert(!fs::exists(fs::path(argv[1]) / kSnapshotToken));
    std::cout << "Save files: long profile paths, legacy buffers, checksum damage/truncation and stale-error recovery passed\n";
}
