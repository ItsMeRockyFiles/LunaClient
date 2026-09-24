#include "../environment.h"
#include "../../lua/lua.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UNC debug.* Library Extensions
//  UNC spec: debug.getconstants, debug.setconstant, debug.getupvalues,
//             debug.setupvalue, debug.getprotos, debug.getstack,
//             debug.setstack, debug.getinfo
// ─────────────────────────────────────────────────────────────────────────────

// Luau Proto struct layout (abbreviated)
// Used for L-closure constant/upvalue/proto manipulation
struct TValue {
    union {
        double   n;
        int      b;
        void*    gc;
        void*    p;
        float    v[2];
    } value;
    int tt;  // type tag
};

struct LocVar {
    void*  varname; // TString*
    int    startpc;
    int    endpc;
};

struct Proto {
    void*   next;       // GCObject*
    uint8_t tt;
    uint8_t marked;
    uint8_t memcat;
    uint8_t numparams;
    uint8_t is_vararg;
    uint8_t maxstacksize;
    uint8_t flags;
    uint8_t pad;

    TValue*  k;          // constants array
    uint32_t sizek;

    Proto**  p;          // nested protos
    uint32_t sizep;

    // ... instruction array, line info, upvalue names, etc.
    // Offsets within Proto are version-specific; verify with dumper
};

// Get Proto* from an L-closure on the stack
static Proto* get_proto(lua_State* L, int idx) {
    if (lua.iscfunction(L, idx)) return nullptr;
    if (!lua.isfunction(L, idx)) return nullptr;

    // The closure GC object has isC=0, and proto pointer at offset 0x10
    // (after the GC header which is 8 bytes on x64 + 4 flag bytes = 0xC, aligned to 0x10)
    void* cl_ptr = const_cast<void*>(lua.topointer(L, idx));
    if (!cl_ptr) return nullptr;

    // Approximate: cl->proto is at byte offset 0x10 in Luau LClosure
    return *reinterpret_cast<Proto**>(static_cast<uint8_t*>(cl_ptr) + 0x10);
}

// ── debug.getconstants(fn) → table ───────────────────────────────────────────
static int luna_debug_getconstants(lua_State* L) {
    Proto* proto = get_proto(L, 1);
    lua.createtable(L, 0, 0);
    if (!proto || !proto->k) return 1;

    for (uint32_t i = 0; i < proto->sizek; ++i) {
        lua.pushinteger(L, static_cast<int>(i + 1));
        TValue* k = &proto->k[i];
        switch (k->tt) {
            case 0:  lua.pushnil(L); break;                                     // nil
            case 1:  lua.pushboolean(L, k->value.b); break;                    // boolean
            case 3:  lua.pushnumber(L, k->value.n); break;                     // number
            case 5:  {                                                           // string
                auto* s = static_cast<uint8_t*>(k->value.gc);
                uint32_t len = *reinterpret_cast<uint32_t*>(s + 0xC);
                const char* data = reinterpret_cast<const char*>(s + sizeof(void*) + 8);
                lua.pushlstring(L, data, len);
                break;
            }
            default: lua.pushnil(L); break;
        }
        lua.settable(L, -3);
    }
    return 1;
}

// ── debug.setconstant(fn, index, value) ──────────────────────────────────────
static int luna_debug_setconstant(lua_State* L) {
    Proto* proto = get_proto(L, 1);
    if (!proto || !proto->k) return 0;

    int idx = static_cast<int>(lua.tointeger(L, 2)) - 1; // 1-indexed → 0-indexed
    if (idx < 0 || static_cast<uint32_t>(idx) >= proto->sizek) return 0;

    TValue* k = &proto->k[idx];
    int vtype = lua.type(L, 3);
    switch (vtype) {
        case LUA_TNUMBER:  k->tt = 3; k->value.n = lua.tonumber(L, 3); break;
        case LUA_TBOOLEAN: k->tt = 1; k->value.b = lua.toboolean(L, 3); break;
        case LUA_TNIL:     k->tt = 0; break;
        default: break; // string/instance constants are more complex — TODO
    }
    return 0;
}

// ── debug.getupvalues(fn) → table ────────────────────────────────────────────
static int luna_debug_getupvalues(lua_State* L) {
    lua.createtable(L, 0, 0);

    if (!lua.isfunction(L, 1)) return 1;

    void* cl_ptr = const_cast<void*>(lua.topointer(L, 1));
    if (!cl_ptr) return 1;

    uint8_t nupvalues = *reinterpret_cast<uint8_t*>(static_cast<uint8_t*>(cl_ptr) + 0x04);
    if (nupvalues == 0) return 1;

    // Upvalues start at offset 0x18 in the closure struct
    // Each upvalue slot is a TValue (16 bytes on x64)
    constexpr size_t UPVAL_START = 0x18;
    constexpr size_t TVALUE_SIZE = 16;

    for (uint8_t i = 0; i < nupvalues; ++i) {
        lua.pushinteger(L, static_cast<int>(i + 1));

        uintptr_t uv_addr = reinterpret_cast<uintptr_t>(cl_ptr) + UPVAL_START + i * TVALUE_SIZE;
        int tt = *reinterpret_cast<int*>(uv_addr + 8);

        switch (tt) {
            case LUA_TNIL:     lua.pushnil(L); break;
            case LUA_TBOOLEAN: lua.pushboolean(L, *reinterpret_cast<int*>(uv_addr)); break;
            case LUA_TNUMBER:  lua.pushnumber(L, *reinterpret_cast<double*>(uv_addr)); break;
            case LUA_TSTRING: {
                void* gc = *reinterpret_cast<void**>(uv_addr);
                if (gc) {
                    uint32_t len = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(gc) + 0x0C);
                    const char* data = reinterpret_cast<const char*>(static_cast<uint8_t*>(gc) + 0x10);
                    lua.pushlstring(L, data, len);
                } else {
                    lua.pushnil(L);
                }
                break;
            }
            default: lua.pushnil(L); break;
        }
        lua.settable(L, -3);
    }
    return 1;
}

// ── debug.setupvalue(fn, index, value) ───────────────────────────────────────
static int luna_debug_setupvalue(lua_State* L) {
    if (!lua.isfunction(L, 1)) return 0;

    int idx = static_cast<int>(lua.tointeger(L, 2)) - 1;
    void* cl_ptr = const_cast<void*>(lua.topointer(L, 1));
    if (!cl_ptr) return 0;

    uint8_t nupvalues = *reinterpret_cast<uint8_t*>(static_cast<uint8_t*>(cl_ptr) + 0x04);
    if (idx < 0 || static_cast<uint8_t>(idx) >= nupvalues) return 0;

    constexpr size_t UPVAL_START = 0x18;
    constexpr size_t TVALUE_SIZE = 16;
    uintptr_t uv_addr = reinterpret_cast<uintptr_t>(cl_ptr) + UPVAL_START + idx * TVALUE_SIZE;

    int vtype = lua.type(L, 3);
    switch (vtype) {
        case LUA_TNIL:
            *reinterpret_cast<int*>(uv_addr + 8) = LUA_TNIL;
            break;
        case LUA_TBOOLEAN:
            *reinterpret_cast<int*>(uv_addr) = lua.toboolean(L, 3);
            *reinterpret_cast<int*>(uv_addr + 8) = LUA_TBOOLEAN;
            break;
        case LUA_TNUMBER:
            *reinterpret_cast<double*>(uv_addr) = lua.tonumber(L, 3);
            *reinterpret_cast<int*>(uv_addr + 8) = LUA_TNUMBER;
            break;
        default: break;
    }
    return 0;
}

// ── debug.getprotos(fn) → table ──────────────────────────────────────────────
static int luna_debug_getprotos(lua_State* L) {
    Proto* proto = get_proto(L, 1);
    lua.createtable(L, 0, 0);
    if (!proto || !proto->p) return 1;

    for (uint32_t i = 0; i < proto->sizep; ++i) {
        lua.pushinteger(L, static_cast<int>(i + 1));
        // Return sub-proto as lightuserdata for now
        lua.pushlightuserdata(L, proto->p[i]);
        lua.settable(L, -3);
    }
    return 1;
}

// ── debug.getstack(thread | level, index?) → value | table ───────────────────
static int luna_debug_getstack(lua_State* L) {
    // Simplified: return stack at given index of current thread
    int idx = lua.isnumber(L, 1) ? static_cast<int>(lua.tointeger(L, 1)) : 1;
    lua.pushvalue(L, idx);
    return 1;
}

// ── debug.setstack(level, index, value) ──────────────────────────────────────
static int luna_debug_setstack(lua_State* L) {
    int level = lua.isnumber(L, 1) ? static_cast<int>(lua.tointeger(L, 1)) : 1;
    // Push value (arg 3) and replace the stack slot
    // This is a simplified version — full implementation would walk call frames
    if (lua.top(L) >= 3) {
        lua.pushvalue(L, 3);
        lua.replace(L, level);
    }
    return 0;
}

// ── debug.getinfo(fn | level) → table ────────────────────────────────────────
static int luna_debug_getinfo(lua_State* L) {
    lua.createtable(L, 0, 0);

    bool is_fn = lua.isfunction(L, 1);
    Proto* proto = is_fn ? get_proto(L, 1) : nullptr;

    lua.pushstring(L, "what");
    lua.pushstring(L, proto ? "Lua" : "C");
    lua.settable(L, -3);

    lua.pushstring(L, "numparams");
    lua.pushnumber(L, proto ? proto->numparams : 0);
    lua.settable(L, -3);

    lua.pushstring(L, "is_vararg");
    lua.pushboolean(L, proto ? proto->is_vararg : 0);
    lua.settable(L, -3);

    return 1;
}

namespace environment {

void register_debug(lua_State* L) {
    // Get or create debug table
    lua.getglobal(L, "debug");
    if (!lua.istable(L, -1)) {
        lua.pop(L, 1);
        lua.createtable(L, 0, 0);
    }

    auto set = [&](const char* name, lua_CFunction fn) {
        lua.pushstring(L, name);
        lua.pushcclosure(L, fn, name, 0);
        lua.settable(L, -3);
    };

    set("getconstants",  luna_debug_getconstants);
    set("setconstant",   luna_debug_setconstant);
    set("getupvalues",   luna_debug_getupvalues);
    set("setupvalue",    luna_debug_setupvalue);
    set("getprotos",     luna_debug_getprotos);
    set("getstack",      luna_debug_getstack);
    set("setstack",      luna_debug_setstack);
    set("getinfo",       luna_debug_getinfo);

    lua.setglobal(L, "debug");
}

} // namespace environment
