#include "fast/VertexUploadCursor.h"
#include <array>
#include <cstdio>
#include <cstring>

int main() {
    Fast::VertexUploadCursor cursor;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool okay) { ++checks; failures += !okay; };
    auto range = cursor.Reserve(4);
    check(range.valid && range.discard && range.offset == 0);
    range = cursor.Reserve(12);
    check(range.valid && !range.discard && range.offset == 16);
    range = cursor.Reserve(cursor.Capacity - 32);
    check(range.valid && !range.discard && range.offset == 32 && cursor.next == cursor.Capacity);
    range = cursor.Reserve(4);
    check(range.valid && range.discard && range.offset == 0);
    const auto before = cursor.next;
    check(!cursor.Reserve(cursor.Capacity + 1).valid && cursor.next == before);
    cursor.Reset();
    check(cursor.Reserve(cursor.Capacity).discard && cursor.next == cursor.Capacity);

    std::array<unsigned char, Fast::VertexUploadCursor::Capacity> memory{};
    bool streamed = true;
    unsigned previousEnd = 0, wraps = 0;
    // Vary batch sizes, vertex strides and frame boundaries. Each live range
    // must remain disjoint and recover exactly the submitted vertex bytes.
    for (unsigned draw = 1; draw <= 10000; ++draw) {
        if (draw % 131 == 0) cursor.Reset();
        const unsigned bytes = (1 + draw * 37 % 768) * 4;
        range = cursor.Reserve(bytes);
        if (range.discard) { previousEnd = 0; ++wraps; }
        streamed &= range.valid && range.offset % 16 == 0 && range.offset >= previousEnd &&
                    range.offset + bytes <= cursor.Capacity;
        if (!streamed) break;
        std::memset(memory.data() + range.offset, draw % 251, bytes);
        for (unsigned byte = 0; byte < bytes; ++byte) streamed &= memory[range.offset + byte] == draw % 251;
        previousEnd = range.offset + bytes;
    }
    check(streamed && wraps > 100 && wraps < 10000);
    std::printf("Vertex upload: %u checks, %u failures, %u discard generations for 10000 batches\n", checks, failures, wraps);
    return failures ? 2 : 0;
}
