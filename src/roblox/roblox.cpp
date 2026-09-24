#include "roblox.h"
#include "../utils/memory.h"
#include "../../offsets.h"
#include <cstdint>

// Global lua API instance
lua_api lua;

namespace roblox {

// ── Lua function pointer offsets from Roblox binary ──────────────────────────
// These need to be pattern-scanned or manually updated per Roblox version.
// Marked with TODO for the user to fill in via pattern scan or dumper output.

// Helper: resolve a function from RBX base + offset
static inline uintptr_t fn(uintptr_t offset) {
    return memory::rebase(offset);
}

bool initialize(lua_State* /*L*/) {
    // ── Resolve Luau function pointers ───────────────────────────────────────
    // TODO: Update these offsets for version-8bb29a9b986b4cfa
    //       Use a Luau function dumper or cross-reference with public dumps.
    //       These are placeholder offsets — replace with real ones.
    //
    // Example layout (x64, Roblox 2024+):
    //   lua.gettop    = reinterpret_cast<fn_lua_gettop>   (fn(0x???????));
    //
    // For now we use a pattern scan approach to find lua_gettop as anchor:
    //   Pattern: 48 8B 41 10 2B 41 18 C1 F8 03 C3  (lua_gettop x64 typical)

    // ── lua_gettop ───────────────────────────────────────────────────────────
    // Typical pattern in Luau: mov rax,[rcx+10h]; sub eax,[rcx+18h]; sar eax,3; ret
    static const uint8_t pat_gettop[] = { 0x48, 0x8B, 0x41, 0x10, 0x2B, 0x41, 0x18, 0xC1, 0xF8, 0x03, 0xC3 };
    uintptr_t gettop_addr = memory::pattern_scan(nullptr, pat_gettop, "xxxxxxxxxxx");
    if (gettop_addr) {
        lua.gettop = reinterpret_cast<fn_lua_gettop>(gettop_addr);
    }

    // NOTE: The rest of the Lua function pointers are offset from gettop or
    // found via their own patterns. A full pattern set should be maintained in
    // a separate signatures.h file once verified against the target version.
    //
    // For a working executor, resolve at minimum:
    //   gettop, settop, pushvalue, pushnil, pushnumber, pushinteger,
    //   pushlstring, pushstring, pushboolean, pushcclosure,
    //   tonumber, tointeger, toboolean, tolstring, touserdata, tothread,
    //   type, typename_, gettable, settable, getfield, setfield,
    //   rawget, rawset, rawgeti, rawseti, createtable, newuserdata,
    //   getmetatable, setmetatable, call, pcall, loadbuffer, error,
    //   next, concat, gc, newthread, ref, unref

    return (lua.gettop != nullptr);
}

lua_State* get_script_state() {
    // Walk TaskScheduler jobs to find "WaitingHybridScriptsJob"
    // then grab the lua_State* from within it.
    // This is version-specific and must be adapted to the job structure.
    uintptr_t ts = get_task_scheduler();
    if (!ts) return nullptr;

    uintptr_t job_start = memory::read<uintptr_t>(ts + offsets::TaskScheduler::JobStart);
    uintptr_t job_end   = memory::read<uintptr_t>(ts + offsets::TaskScheduler::JobEnd);
    if (!job_start || !job_end) return nullptr;

    constexpr size_t ptr_size = sizeof(uintptr_t);
    for (uintptr_t cur = job_start; cur < job_end; cur += ptr_size) {
        uintptr_t job = memory::read<uintptr_t>(cur);
        if (!job) continue;

        // Read job name string
        uintptr_t name_addr = memory::read<uintptr_t>(job + offsets::TaskScheduler::JobName);
        if (!name_addr) continue;
        std::string name = memory::read_string(name_addr);

        if (name == "WaitingHybridScriptsJob") {
            // lua_State* is typically at job + 0x1C8 (version-dependent)
            // TODO: verify for version-8bb29a9b986b4cfa
            return memory::read<lua_State*>(job + 0x1C8);
        }
    }

    return nullptr;
}

} // namespace roblox
