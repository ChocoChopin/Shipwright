// Synthetic checks of the resource row copier used by the I4 importer.
// No engine process, asset payload, or invalid memory dereference is needed.
#include "fast/TextureCopy.h"
#include <array>
#include <cstdio>

namespace {
int checks = 0;
bool Check(bool condition, const char* name) {
    ++checks;
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", name);
    return condition;
}
bool All(const uint8_t* data, size_t count, uint8_t value) {
    for (size_t i = 0; i < count; ++i) if (data[i] != value) return false;
    return true;
}
}

int main() {
    bool ok = true;
    // Match the failed resource's 80-byte header / 64-byte image layout.
    std::array<uint8_t, 144> file;
    for (size_t i = 0; i < file.size(); ++i) file[i] = static_cast<uint8_t>(i * 37 + 11);
    const uint8_t* image = file.data() + 80;
    Fast::TextureResourceSpan span{ image, 64, file.data(), file.size(), true };
    std::array<uint8_t, 130> output;
    output.fill(0xA5);
    auto result = Fast::CopyTextureResourceRows(output.data() + 1, 128, 8, 16, image, 8, span);
    ok &= Check(result.valid && result.copiedBytes == 64, "128-byte I4 request copies 64 represented bytes");
    ok &= Check(std::memcmp(output.data() + 1, image, 64) == 0, "represented I4 texels remain byte exact");
    ok &= Check(All(output.data() + 65, 64, 0), "unrepresented I4 texels have zero intensity and alpha");
    ok &= Check(output.front() == 0xA5 && output.back() == 0xA5, "128-byte destination guards remain intact");
    ok &= Check(Fast::AvailableTextureResourceBytes(image, span) == 64, "retained file bounds the image");

    // A complete resource requires no fallback; packing keeps all row bytes.
    std::array<uint8_t, 128> full;
    for (size_t i = 0; i < full.size(); ++i) full[i] = static_cast<uint8_t>(i * 19 + 3);
    auto complete = Fast::TextureResourceSpan{ full.data(), full.size(), nullptr, 0, false };
    result = Fast::CopyTextureResourceRows(output.data() + 1, 128, 8, 16, full.data(), 8, complete);
    ok &= Check(result.valid && result.copiedBytes == 128, "complete image copies every byte");
    ok &= Check(std::memcmp(output.data() + 1, full.data(), 128) == 0, "complete I4 image stays exact");

    std::array<uint8_t, 12> strided{ 0x12, 0x34, 0xEE, 0xEE, 0x56, 0x78, 0xEE, 0xEE, 0x9A, 0xBC, 0xEE, 0xEE };
    auto rows = Fast::TextureResourceSpan{ strided.data(), strided.size(), nullptr, 0, false };
    output.fill(0xA5);
    result = Fast::CopyTextureResourceRows(output.data() + 1, 6, 2, 3, strided.data(), 4, rows);
    const std::array<uint8_t, 6> expected{ 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC };
    ok &= Check(result.valid && result.copiedBytes == 6, "strided rows copy only the requested columns");
    ok &= Check(std::memcmp(output.data() + 1, expected.data(), expected.size()) == 0, "row padding is excluded");
    ok &= Check(output.front() == 0xA5 && All(output.data() + 7, 123, 0xA5), "strided write keeps surrounding bytes");
    rows.imageBytes = 9;
    result = Fast::CopyTextureResourceRows(output.data() + 1, 6, 2, 3, strided.data(), 4, rows);
    ok &= Check(result.valid && result.copiedBytes == 5, "partial final row counts represented bytes");
    ok &= Check(std::memcmp(output.data() + 1, expected.data(), 5) == 0 && output[6] == 0,
                "partial row keeps its prefix and zeroes its suffix");

    span.imageBytes = 128;
    result = Fast::CopyTextureResourceRows(output.data() + 1, 128, 8, 16, image, 8, span);
    ok &= Check(result.valid && result.copiedBytes == 64 && All(output.data() + 65, 64, 0),
                "owned file caps an oversized declared image");
    span.imageBytes = 64;
    span.hasOwnedBuffer = false;
    result = Fast::CopyTextureResourceRows(output.data() + 1, 128, 8, 16, image, 8, span);
    ok &= Check(result.valid && result.copiedBytes == 64, "separate image allocation remains bounded");
    result = Fast::CopyTextureResourceRows(output.data() + 1, 8, 4, 2, image + 60, 4, span);
    ok &= Check(result.valid && result.copiedBytes == 4 && std::memcmp(output.data() + 1, image + 60, 4) == 0 &&
                All(output.data() + 5, 4, 0), "source offset at final row respects remaining extent");
    result = Fast::CopyTextureResourceRows(output.data() + 1, 8, 4, 2, image + 64, 4, span);
    ok &= Check(result.valid && result.copiedBytes == 0 && All(output.data() + 1, 8, 0), "one-past source is never read");
    result = Fast::CopyTextureResourceRows(output.data() + 1, 8, 4, 2, nullptr, 4, span);
    ok &= Check(result.valid && result.copiedBytes == 0 && All(output.data() + 1, 8, 0), "null source has defined empty rows");
    result = Fast::CopyTextureResourceRows(output.data() + 1, 8, 4, 2, full.data(), 4, span);
    ok &= Check(result.valid && result.copiedBytes == 0 && All(output.data() + 1, 8, 0), "unrelated allocation is not read");

    output.fill(0xA5);
    result = Fast::CopyTextureResourceRows(output.data() + 1, 127, 8, 16, image, 8, span);
    ok &= Check(!result.valid && All(output.data(), output.size(), 0xA5), "short destination rejected before any write");
    result = Fast::CopyTextureResourceRows(output.data(), output.size(), 1, 3, image,
                                          (std::numeric_limits<size_t>::max)(), span);
    ok &= Check(!result.valid && All(output.data(), output.size(), 0xA5), "overflowing source row offsets rejected");
    result = Fast::CopyTextureResourceRows(output.data(), output.size(), (std::numeric_limits<size_t>::max)(),
                                          2, image, 8, span);
    ok &= Check(!result.valid && All(output.data(), output.size(), 0xA5), "overflowing packed size rejected");
    result = Fast::CopyTextureResourceRows(nullptr, 128, 8, 16, image, 8, span);
    ok &= Check(!result.valid, "absent destination rejected");
    result = Fast::CopyTextureResourceRows(output.data(), output.size(), 0, 16, image, 8, span);
    ok &= Check(result.valid && result.copiedBytes == 0 && All(output.data(), output.size(), 0xA5), "empty rows are a no-op");
    result = Fast::CopyTextureResourceRows(output.data(), output.size(), 8, 0, image, 8, span);
    ok &= Check(result.valid && result.copiedBytes == 0 && All(output.data(), output.size(), 0xA5), "zero rows are a no-op");

    std::printf("Texture bounds: %s; %d checks\n", ok ? "PASS" : "FAIL", checks);
    return ok ? 0 : 1;
}
