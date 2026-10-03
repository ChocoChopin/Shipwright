#include "CrashHandlerExt.h"
#include "variables.h"
#include "z64.h"
#include "z64actor.h"
#include <stdio.h>
#include <array>
#include "soh/ActorDB.h"
#include "soh/NativeSimulationTest.h"
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <ship/Context.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#define WRITE_VAR_LINE(buff, len, varName, varValue) \
    append_str(buff, len, varName);                  \
    append_line(buff, len, varValue);
#define WRITE_VAR(buff, len, varName, varValue) \
    append_str(buff, len, varName);             \
    append_str(buff, len, varValue);

extern "C" PlayState* gPlayState;

static std::array<const char*, ACTORCAT_MAX> sCatToStrArray{
    "SWITCH", "BG", "PLAYER", "EXPLOSIVE", "NPC", "ENEMY", "PROP", "ITEMACTION", "MISC", "BOSS", "DOOR", "CHEST",
};

#define DEFINE_SCENE(_1, _2, enumName, _4, _5, _6) #enumName,

static std::array<const char*, SCENE_ID_MAX> sSceneIdToStrArray{
#include "tables/scene_table.h"
};

#undef DEFINE_SCENE

static void append_str(char* buf, size_t* len, const char* str) {
    while (*str != '\0')
        buf[(*len)++] = *str++;
}

static void append_line(char* buf, size_t* len, const char* str) {
    while (*str != '\0')
        buf[(*len)++] = *str++;
    buf[(*len)++] = '\n';
}

static void CrashHandler_WriteActorData(char* buffer, size_t* pos) {
    for (unsigned int i = 0; i < ACTORCAT_MAX; i++) {

        ActorListEntry* entry = &gPlayState->actorCtx.actorLists[i];
        Actor* cur;

        if (entry->length == 0) {
            continue;
        }
        WRITE_VAR_LINE(buffer, pos, "  Category: ", sCatToStrArray[i]);
        cur = entry->head;
        while (cur != nullptr) {
            std::string actorLine = "    ";
            actorLine += ActorDB::Instance->RetrieveEntry(cur->id).entry.valid
                             ? ActorDB::Instance->RetrieveEntry(cur->id).entry.desc
                             : "???";
            actorLine += " (" + std::to_string(cur->params) + ")";
            append_line(buffer, pos, actorLine.c_str());

            cur = cur->next;
        }
    }
}

static void CrashHandler_WriteNativeSimGfxData(char* buffer, size_t* pos) {
    if (!NativeSimTest_IsEnabled()) {
        return;
    }

    append_line(buffer, pos, "Native simulation graphics diagnostics (addresses excluded from replay hashes):");
    char line[1024];
    Fast::F3DGfx command{};
    const Fast::F3DGfx* current = Fast::g_exec_stack.cmd_stack.empty() ? nullptr : Fast::g_exec_stack.cmd_stack.top();
    bool commandReadable = false;
    if (current != nullptr) {
#ifdef _WIN32
        // Copy only the interpreter's current stack entry. A bad command pointer
        // must not cause a second access violation while reporting the first.
        SIZE_T copied = 0;
        commandReadable = ReadProcessMemory(GetCurrentProcess(), current, &command, sizeof(command), &copied) &&
                          copied == sizeof(command);
#else
        command = *current;
        commandReadable = true;
#endif
    }
    snprintf(line, sizeof(line), "  current_command=0x%016llX readable=%d stack_depth=%llu",
             static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(current)), commandReadable,
             static_cast<unsigned long long>(Fast::g_exec_stack.cmd_stack.size()));
    append_line(buffer, pos, line);
    const unsigned opcode = static_cast<unsigned>((command.words.w0 >> 24) & 0xFF);
    const unsigned tile = static_cast<unsigned>((command.words.w1 >> 24) & 7);
    const unsigned highIndex = static_cast<unsigned>((command.words.w1 >> 14) & 0x3FF);
    const bool isTlut = commandReadable && opcode == 0xF0;
    if (commandReadable) {
        snprintf(line, sizeof(line), "  w0=0x%016llX w1=0x%016llX opcode=0x%02X is_tlut=%d",
                 static_cast<unsigned long long>(command.words.w0),
                 static_cast<unsigned long long>(command.words.w1), opcode, isTlut);
        append_line(buffer, pos, line);
    }

    auto* context = Ship::Context::GetRawInstance();
    auto window = context ? std::dynamic_pointer_cast<Fast::Fast3dWindow>(context->GetWindow()) : nullptr;
    auto interpreter = window ? window->GetInterpreterWeak().lock() : nullptr;
    if (!interpreter || !interpreter->mRdp) {
        append_line(buffer, pos, "  interpreter/RDP unavailable");
        return;
    }
    const auto& load = interpreter->mRdp->texture_to_load;
    const uintptr_t source = reinterpret_cast<uintptr_t>(load.addr);
    snprintf(line, sizeof(line), "  texture_source=0x%016llX size_code=%u width=%u flags=0x%08X",
             static_cast<unsigned long long>(source), static_cast<unsigned>(load.siz), load.width, load.tex_flags);
    append_line(buffer, pos, line);
    const uint32_t requestedBytes = isTlut ? (highIndex + 1) * 2 : 0;
    if (isTlut) {
        snprintf(line, sizeof(line), "  tlut_tile=%u tmem=%u high_index=%u requested_bytes=%u end_exclusive=0x%016llX end_wrap=%d",
                 tile, static_cast<unsigned>(interpreter->mRdp->texture_tile[tile].tmem), highIndex, requestedBytes,
                 static_cast<unsigned long long>(source + requestedBytes), source > UINTPTR_MAX - requestedBytes);
        append_line(buffer, pos, line);
    }
    // Read metadata held by the interpreter, never bytes at texture_source.
    const auto& resource = load.raw_tex_metadata.resource;
    if (!resource) {
        append_line(buffer, pos, "  texture_resource=none (raw/segmented source)");
        return;
    }
    const auto initData = resource->GetInitData();
    snprintf(line, sizeof(line), "  texture_resource=%.900s", initData ? initData->Path.c_str() : "<no init data>");
    append_line(buffer, pos, line);
    const uintptr_t image = reinterpret_cast<uintptr_t>(resource->ImageData);
    const uint32_t imageSize = resource->ImageDataSize;
    const bool sourceInImage = source >= image && source - image <= imageSize;
    snprintf(line, sizeof(line), "  image_data=0x%016llX image_bytes=%u end_exclusive=0x%016llX end_wrap=%d width=%u height=%u source_in_range=%d requested_fits=%d",
             static_cast<unsigned long long>(image), imageSize, static_cast<unsigned long long>(image + imageSize),
             image > UINTPTR_MAX - imageSize, static_cast<unsigned>(resource->Width),
             static_cast<unsigned>(resource->Height), sourceInImage,
             sourceInImage && requestedBytes <= imageSize - (source - image));
    append_line(buffer, pos, line);
    if (resource->mImageBuffer) {
        const uintptr_t start = reinterpret_cast<uintptr_t>(resource->mImageBuffer->data());
        const size_t bytes = resource->mImageBuffer->size();
        snprintf(line, sizeof(line), "  owned_buffer=0x%016llX bytes=%llu end_exclusive=0x%016llX end_wrap=%d",
                 static_cast<unsigned long long>(start), static_cast<unsigned long long>(bytes),
                 static_cast<unsigned long long>(start + bytes), start > UINTPTR_MAX - bytes);
        append_line(buffer, pos, line);
    }
}

extern "C" void CrashHandler_PrintSohData(char* buffer, size_t* pos) {
    char intCharBuffer[16];
    append_line(buffer, pos, "Build Information:");
    WRITE_VAR_LINE(buffer, pos, "  Game Version: ", (const char*)gBuildVersion);
    WRITE_VAR_LINE(buffer, pos, "  Git Branch: ", (const char*)gGitBranch);
    WRITE_VAR_LINE(buffer, pos, "  Git Commit: ", (const char*)gGitCommitHash);
    WRITE_VAR_LINE(buffer, pos, "  Build Date: ", (const char*)gBuildDate);

    if (gPlayState != nullptr) {
        WRITE_VAR_LINE(buffer, pos, "Scene: ", sSceneIdToStrArray[gPlayState->sceneNum]);

        snprintf(intCharBuffer, sizeof(intCharBuffer), "%i", gPlayState->roomCtx.curRoom.num);
        WRITE_VAR_LINE(buffer, pos, "Room: ", intCharBuffer);

        append_line(buffer, pos, "Actors:");
        CrashHandler_WriteActorData(buffer, pos);

        append_line(buffer, pos, "GFX Stack:");
        for (auto& disp : Fast::g_exec_stack.disp_stack) {
            std::string line = "  ";
            line += disp.file;
            line += ":";
            line += std::to_string(disp.line);
            append_line(buffer, pos, line.c_str());
        }
    }
    CrashHandler_WriteNativeSimGfxData(buffer, pos);
}
