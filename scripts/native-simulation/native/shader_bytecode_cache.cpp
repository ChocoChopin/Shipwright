#include "fast/ShaderBytecodeCache.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    namespace cache = Fast::ShaderBytecodeCache;
    const std::filesystem::path directory(argv[1]);
    unsigned checks = 0, failures = 0;
    auto check = [&](bool okay) { ++checks; if (!okay) ++failures; };
    cache::Bytecode original{{'D','X','B','C'}, {'D','X','B','C'}}, loaded;
    original.vertex.resize(64, 7); original.pixel.resize(96, 9);
    // Synthetic byte strings exercise storage only; never submitted to a driver.
    check(cache::Write(directory, "source/options-a", original));
    check(cache::Read(directory, "source/options-a", loaded) &&
        loaded.vertex == original.vertex && loaded.pixel == original.pixel);
    check(!cache::Read(directory, "source/options-b", loaded));
    std::filesystem::copy_file(cache::Entry(directory, "source/options-a"),
        cache::Entry(directory, "source/options-b"));
    check(!cache::Read(directory, "source/options-b", loaded)); // exact identity, even at the expected filename
    {
        std::fstream file(cache::Entry(directory, "source/options-a"), std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(-1, std::ios::end); file.put(1);
    }
    check(!cache::Read(directory, "source/options-a", loaded)); // damaged payload
    check(cache::Write(directory, "source/options-a", original));
    std::filesystem::resize_file(cache::Entry(directory, "source/options-a"), 28);
    check(!cache::Read(directory, "source/options-a", loaded)); // interrupted write
    auto oversized = original; oversized.vertex.resize(cache::MaxEntryBytes);
    check(!cache::Write(directory, "oversized", oversized));
    check(!cache::Write(cache::Entry(directory, "source/options-b"), "not-a-directory", original));
    std::printf("{\"checks\":%u,\"failures\":%u}\n", checks, failures);
    return failures ? 2 : 0;
}
