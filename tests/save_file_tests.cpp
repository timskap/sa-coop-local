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
    assert(!Valid({}) && !Valid({'B', 'L', 'O', 'C', 'K'}));
    auto corrupted = original;
    corrupted[1000] ^= 1;
    assert(!Valid(corrupted));
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
