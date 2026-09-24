#pragma once
#include <cstdint>

// ─────────────────────────────────────────────
//  Luau type definitions (compatible with Roblox's embedded Luau)
//  Roblox uses a modified Luau fork based on Lua 5.1 with custom extensions.
//  We replicate the ABI-compatible type layout here without including
//  Roblox's proprietary headers.
// ─────────────────────────────────────────────

// Lua integer / number types
using lua_Integer  = int;
using lua_Unsigned = unsigned int;
using lua_Number   = double;

// Forward declarations
struct lua_State;
struct lua_Debug;
struct Proto;
struct Closure;
struct Table;
struct TValue;
struct GCObject;

// Lua C function signature
using lua_CFunction    = int(*)(lua_State* L);
using lua_Alloc        = void*(*)(void* ud, void* ptr, size_t osize, size_t nsize);
using lua_Continuation = int(*)(lua_State* L, int status);

// ── Lua value tags ────────────────────────────────────────────────────────────
enum LuaType : int {
    LUA_TNONE          = -1,
    LUA_TNIL           =  0,
    LUA_TBOOLEAN       =  1,
    LUA_TLIGHTUSERDATA =  2,
    LUA_TNUMBER        =  3,
    LUA_TVECTOR        =  4,
    LUA_TSTRING        =  5,
    LUA_TTABLE         =  6,
    LUA_TFUNCTION      =  7,
    LUA_TUSERDATA      =  8,
    LUA_TTHREAD        =  9,
    LUA_TBUFFER        = 10,
};

// ── Pseudo-indices ────────────────────────────────────────────────────────────
constexpr int LUA_REGISTRYINDEX = -10000;
constexpr int LUA_ENVIRONINDEX  = -10001;
constexpr int LUA_GLOBALSINDEX  = -10002;

// ── Status codes ─────────────────────────────────────────────────────────────
constexpr int LUA_OK        = 0;
constexpr int LUA_YIELD     = 1;
constexpr int LUA_ERRRUN    = 2;
constexpr int LUA_ERRSYNTAX = 3;
constexpr int LUA_ERRMEM    = 4;
constexpr int LUA_ERRERR    = 5;

// ── GC opcodes ───────────────────────────────────────────────────────────────
constexpr int LUA_GCSTOP       = 0;
constexpr int LUA_GCRESTART    = 1;
constexpr int LUA_GCCOLLECT    = 2;
constexpr int LUA_GCCOUNT      = 3;
constexpr int LUA_GCCOUNTB     = 4;
constexpr int LUA_GCSTEP       = 5;
constexpr int LUA_GCSETPAUSE   = 6;
constexpr int LUA_GCSETSTEPMUL = 7;
constexpr int LUA_GCISRUNNING  = 9;

// ── Coroutine status ─────────────────────────────────────────────────────────
enum class CoroStatus : int {
    Normal    = 0,
    Running   = 1,
    Suspended = 2,
    Dead      = 3,
};

// ── lua_Debug ────────────────────────────────────────────────────────────────
struct lua_Debug {
    const char* name;
    const char* what;
    const char* source;
    const char* short_src;
    int         currentline;
    int         defined_line;
    int         up_val_count;
    int         param_count;
    bool        is_vararg;
    bool        is_tail_call;
};

// ── TString (GC string) ──────────────────────────────────────────────────────
struct TString {
    GCObject* next;
    uint8_t   tt;
    uint8_t   marked;
    uint8_t   memcat;
    uint8_t   unk;
    unsigned int hash;
    unsigned int len;
    // char data[] follows
    const char* data() const {
        return reinterpret_cast<const char*>(this + 1);
    }
};
