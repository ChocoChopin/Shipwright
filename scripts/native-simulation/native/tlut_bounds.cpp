// Native regression checks for the production TLUT resource-copy helper.
// Synthetic bytes only; no game assets, renderer, or intentional crashing path.
#include "fast/TlutCopy.h"
#include <array>
#include <cstdio>
#include <vector>

namespace {
int checks = 0;
bool Check(bool condition, const char* name) {
    ++checks;
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", name);
    return condition;
}
bool All(const uint8_t* data, size_t count, uint8_t expected) {
    for (size_t i = 0; i < count; ++i) if (data[i] != expected) return false;
    return true;
}
void Fill(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i * 37 + 11);
}
}

int main() {
    bool ok = true;
    std::vector<uint8_t> full(512);
    Fill(full);
    Fast::TlutResourceSpan fullSpan{ full.data(), full.size(), full.data(), full.size(), true };
    std::array<uint8_t, 514> output;
    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 256, full.data(), 0, fullSpan) == 256,
                "complete first palette half");
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 257, 256, full.data(), 256, fullSpan) == 256,
                "complete second palette half");
    ok &= Check(std::memcmp(output.data() + 1, full.data(), 512) == 0, "complete palette is byte exact");
    ok &= Check(output.front() == 0xA5 && output.back() == 0xA5, "complete palette destination bounds");

    // Match the captured resource layout: 80-byte header and 216 image bytes.
    std::vector<uint8_t> shortFile(296);
    Fill(shortFile);
    const uint8_t* image = shortFile.data() + 80;
    Fast::TlutResourceSpan shortSpan{ image, 216, shortFile.data(), shortFile.size(), true };
    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 256, image, 0, shortSpan) == 216,
                "short first half copies exactly the declared image");
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 257, 256, image, 256, shortSpan) == 0,
                "short second half has no source bytes");
    ok &= Check(std::memcmp(output.data() + 1, image, 216) == 0, "short defined palette bytes preserved");
    ok &= Check(All(output.data() + 217, 296, 0), "only unrepresented requested suffix is initialized");
    ok &= Check(output.front() == 0xA5 && output.back() == 0xA5, "short palette destination bounds");

    auto span = shortSpan;
    span.imageBytes = 512;
    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 512, image, 0, span) == 216,
                "owned file extent caps an oversized declared image");
    ok &= Check(All(output.data() + 217, 296, 0), "owned extent missing suffix");
    span = fullSpan;
    span.imageBytes = 216;
    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 512, full.data(), 0, span) == 216,
                "declared image caps a larger backing allocation");
    ok &= Check(std::memcmp(output.data() + 1, full.data(), 216) == 0 &&
                All(output.data() + 217, 296, 0) && output.front() == 0xA5 && output.back() == 0xA5,
                "declared extent preserves prefix, initializes suffix and retains destination bounds");

    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 64, image + 100, 80, shortSpan) == 36,
                "source position plus integer offset uses remaining extent");
    ok &= Check(std::memcmp(output.data() + 1, image + 180, 36) == 0 &&
                All(output.data() + 37, 28, 0), "offset prefix and missing tail");
    ok &= Check(output.front() == 0xA5 && All(output.data() + 65, 449, 0xA5),
                "offset write leaves unrelated destination bytes intact");
    ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1, 32, image, 216, shortSpan) == 0 &&
                All(output.data() + 1, 32, 0), "offset at image end is empty");

    // CI4 callers request only their affected subrange; both halves retain others.
    for (size_t base : { size_t(16), size_t(256 + 224) }) {
        output.fill(0xA5);
        ok &= Check(Fast::CopyTlutResourceBytes(output.data() + 1 + base, 32, image + 210, 0, shortSpan) == 6,
                    "partial palette writes only available requested bytes");
        ok &= Check(All(output.data(), 1 + base, 0xA5) &&
                    std::memcmp(output.data() + 1 + base, image + 210, 6) == 0 &&
                    All(output.data() + 1 + base + 6, 26, 0) &&
                    All(output.data() + 1 + base + 32, output.size() - 1 - base - 32, 0xA5),
                    "partial palette preserves surrounding entries in either half");
    }
    span = shortSpan;
    span.hasOwnedBuffer = false;
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 256, image, 0, span) == 216,
                "separately owned resource still respects its image extent");
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, nullptr, 0, shortSpan) == 0 &&
                All(output.data(), 16, 0), "absent source yields an empty requested span");
    span = shortSpan;
    span.imageBytes = 0;
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, image, 0, span) == 0 &&
                All(output.data(), 16, 0), "empty image yields an empty requested span");
    span = shortSpan;
    span.ownedBufferBytes = 0;
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, image, 0, span) == 0,
                "empty backing prevents a resource read");
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, full.data(), 0, shortSpan) == 0 &&
                All(output.data(), 16, 0), "a different resource is outside the declared span");
    span = shortSpan;
    span.imageBytes = (std::numeric_limits<size_t>::max)();
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, image, 0, span) == 0 &&
                All(output.data(), 16, 0), "overflowing declared image extent is rejected");
    span = shortSpan;
    span.ownedBufferBytes = (std::numeric_limits<size_t>::max)();
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 16, image, 0, span) == 0 &&
                All(output.data(), 16, 0), "overflowing backing extent is rejected");
    output.fill(0xA5);
    ok &= Check(Fast::CopyTlutResourceBytes(output.data(), 0, image, 0, shortSpan) == 0 &&
                All(output.data(), output.size(), 0xA5), "empty destination request changes nothing");
    ok &= Check(reinterpret_cast<uintptr_t>(Fast::TlutAddressToken(image, 256)) ==
                reinterpret_cast<uintptr_t>(image) + 256, "cache-only token retains numeric identity");
    ok &= Check(Fast::TlutAddressToken(image, (std::numeric_limits<size_t>::max)()) == nullptr,
                "cache-only token rejects address overflow");
    std::printf("TLUT bounds: %s; %d checks\n", ok ? "PASS" : "FAIL", checks);
    return ok ? 0 : 1;
}
