#include "../environment.h"
#include "../../lua/lua.h"
#include "../../roblox/roblox.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Miscellaneous Functions
//  Spec refs:
//    identifyexecutor / getexecutorname
//    isluau
//    setfpscap / getfpscap
//    setclipboard
//    setfflag (UNC optional)
//    request alias registration (actual impl in http.cpp)
// ─────────────────────────────────────────────────────────────────────────────

// ── identifyexecutor() → name: string, version: string ───────────────────────
static int luna_identifyexecutor(lua_State* L) {
    lua.pushstring(L, LUNA_NAME);
    lua.pushstring(L, LUNA_VERSION);
    return 2;
}

// ── getexecutorname() → string  (alias) ──────────────────────────────────────
static int luna_getexecutorname(lua_State* L) {
    lua.pushstring(L, LUNA_NAME);
    return 1;
}

// ── isluau() → boolean ────────────────────────────────────────────────────────
static int luna_isluau(lua_State* L) {
    lua.pushboolean(L, 1);
    return 1;
}

// ── setfpscap(cap: number) ────────────────────────────────────────────────────
// UNC: setfpscap(fps: number) — 0 = uncapped
static int luna_setfpscap(lua_State* L) {
    if (!lua.isnumber(L, 1))
        lua.error(L); // type error

    double fps = lua.tonumber(L, 1);
    // Roblox TaskScheduler stores 1/fps as the step interval
    // 0 = uncapped, otherwise clamp to reasonable range
    if (fps <= 0.0) fps = 0.0;
    roblox::set_fps_cap(fps > 0.0 ? fps : 1e+300);
    return 0;
}

// ── getfpscap() → number ─────────────────────────────────────────────────────
static int luna_getfpscap(lua_State* L) {
    double cap = roblox::get_fps_cap();
    // Convert internal representation back to fps
    if (cap >= 1e+300 || cap <= 0.0)
        lua.pushnumber(L, 0.0); // uncapped
    else
        lua.pushnumber(L, cap);
    return 1;
}

// ── setclipboard(text: string) ────────────────────────────────────────────────
static int luna_setclipboard(lua_State* L) {
    if (!lua.isstring(L, 1))
        lua.error(L);

    const char* text = lua.tostring(L, 1);
    if (!OpenClipboard(nullptr)) return 0;
    EmptyClipboard();

    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
    return 0;
}

// ── getclipboard() → string ───────────────────────────────────────────────────
static int luna_getclipboard(lua_State* L) {
    if (!OpenClipboard(nullptr)) {
        lua.pushstring(L, "");
        return 1;
    }
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) {
        CloseClipboard();
        lua.pushstring(L, "");
        return 1;
    }
    char* text = static_cast<char*>(GlobalLock(hData));
    lua.pushstring(L, text ? text : "");
    GlobalUnlock(hData);
    CloseClipboard();
    return 1;
}

// ── lz4compress / lz4decompress (UNC optional) ───────────────────────────────
// Placeholder — implement when lz4 vendored
static int luna_lz4compress(lua_State* L) {
    lua.pushstring(L, "lz4compress: not implemented");
    lua.error(L);
    return 0;
}

static int luna_lz4decompress(lua_State* L) {
    lua.pushstring(L, "lz4decompress: not implemented");
    lua.error(L);
    return 0;
}

// ── messagebox(text, caption, flags) → number ─────────────────────────────────
static int luna_messagebox(lua_State* L) {
    const char* text    = lua.isstring(L, 1) ? lua.tostring(L, 1) : "";
    const char* caption = lua.isstring(L, 2) ? lua.tostring(L, 2) : "LunaClient";
    int flags           = lua.isnumber(L, 3) ? static_cast<int>(lua.tointeger(L, 3)) : MB_OK;
    int result = MessageBoxA(nullptr, text, caption, flags);
    lua.pushnumber(L, static_cast<double>(result));
    return 1;
}

// ── checkcaller() → boolean ───────────────────────────────────────────────────
// Returns true if caller is the executor's own environment
// Simplified: always returns true when called from our scripts
static int luna_checkcaller(lua_State* L) {
    lua.pushboolean(L, 1);
    return 1;
}

namespace environment {

void register_misc(lua_State* L) {
    lua.register_fn(L, "identifyexecutor",  luna_identifyexecutor);
    lua.register_fn(L, "getexecutorname",   luna_getexecutorname);
    lua.register_fn(L, "isluau",            luna_isluau);
    lua.register_fn(L, "setfpscap",         luna_setfpscap);
    lua.register_fn(L, "getfpscap",         luna_getfpscap);
    lua.register_fn(L, "setclipboard",      luna_setclipboard);
    lua.register_fn(L, "getclipboard",      luna_getclipboard);
    lua.register_fn(L, "messagebox",        luna_messagebox);
    lua.register_fn(L, "checkcaller",       luna_checkcaller);
    lua.register_fn(L, "lz4compress",       luna_lz4compress);
    lua.register_fn(L, "lz4decompress",     luna_lz4decompress);

    // cloneref / compareinstances — instance identity helpers (UNC)
    // Implemented as stubs until Instance userdata is fully mapped
    lua.register_fn(L, "cloneref", [](lua_State* L) -> int {
        lua.pushvalue(L, 1); // return same ref for now
        return 1;
    });
    lua.register_fn(L, "compareinstances", [](lua_State* L) -> int {
        // TODO: compare underlying pointers
        lua.pushboolean(L, 0);
        return 1;
    });
}

} // namespace environment
