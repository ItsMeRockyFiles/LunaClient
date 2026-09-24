#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../lua/lua.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Hooks — MinHook-based function hooks for Roblox internals
// ─────────────────────────────────────────────────────────────────────────────

namespace hooks {

// Initialize MinHook and install all hooks
bool initialize();

// Remove all hooks and uninitialize MinHook
void shutdown();

// ── Hooked functions ──────────────────────────────────────────────────────────

// Script scheduler hook — intercepts script execution to inject our environment
// Hooked at: luaD_call or similar execution entry point
int script_start_hook(lua_State* L, int nargs, int nresults);

// luaO_nilobject or error handler — for catching and logging errors gracefully
void error_hook(lua_State* L, const char* msg);

} // namespace hooks
