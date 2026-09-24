#include "../environment.h"
#include "../../lua/lua.h"
#include "../../roblox/roblox.h"
#include "../../utils/memory.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Closure / Function Functions
//  UNC spec: newcclosure, hookfunction, iscclosure, islclosure,
//             clonefunction, checkcaller, getcallingscript
// ─────────────────────────────────────────────────────────────────────────────

// ── Luau Closure struct layout ────────────────────────────────────────────────
// In Roblox's Luau, a Closure (TValue with tag TFUNCTION) is either:
//   - CClosure: isC == 1, f = lua_CFunction
//   - LClosure: isC == 0, l.p = Proto*
// The GCObject header comes first, followed by:
//   uint8_t isC, nupvalues, stacksize, preload
//   union { struct CClosure; struct LClosure }

struct RbxCClosure {
    void*        next;       // GCObject*
    uint8_t      tt;
    uint8_t      marked;
    uint8_t      memcat;
    uint8_t      isC;        // 1 for CFunction
    uint8_t      nupvalues;
    uint8_t      stacksize;
    uint8_t      preload;
    uint8_t      pad;
    lua_CFunction f;         // the actual C function pointer
    // TValue upvalues[] follow
};

struct RbxLClosure {
    void*    next;
    uint8_t  tt;
    uint8_t  marked;
    uint8_t  memcat;
    uint8_t  isC;       // 0 for Lua closure
    uint8_t  nupvalues;
    uint8_t  stacksize;
    uint8_t  preload;
    uint8_t  pad;
    void*    proto;     // Proto* (Luau proto)
    // TValue upvalues[] follow
};

static RbxCClosure* get_closure_ptr(lua_State* L, int idx) {
    return const_cast<RbxCClosure*>(
        static_cast<const RbxCClosure*>(lua.topointer(L, idx))
    );
}

// ── newcclosure(fn: function) → cclosure ─────────────────────────────────────
// Wraps a Lua function inside a C closure that calls it.
// The wrapped function is stored as an upvalue.
static int luna_newcclosure_call(lua_State* L) {
    // Upvalue 1 = the original Lua function
    // Forward all args to it and return results
    int nargs = lua.top(L);
    lua.pushvalue(L, lua.top(L) + 1); // push upvalue 1 (original fn) — use pseudo-index
    // Actually use rawgeti on registry ref
    // We stored the ref in an upvalue — the closure framework handles this
    // For simplicity: push the upvalue, move it before args, then call
    lua.pushvalue(L, lua.GLOBALSINDEX - 1); // this is lua_upvalueindex(1) = -10003
    lua.insert(L, 1);
    lua.call(L, nargs, -1);
    return lua.top(L);
}

static int luna_newcclosure(lua_State* L) {
    if (!lua.isfunction(L, 1)) {
        lua.pushstring(L, "newcclosure: expected function");
        lua.error(L);
    }
    // Push the target function as an upvalue and create a C closure around it
    lua.pushvalue(L, 1);
    lua.pushcclosure(L, luna_newcclosure_call, "newcclosure_wrapper", 1);
    return 1;
}

// ── iscclosure(fn) → boolean ──────────────────────────────────────────────────
static int luna_iscclosure(lua_State* L) {
    if (!lua.isfunction(L, 1)) { lua.pushboolean(L, 0); return 1; }
    lua.pushboolean(L, lua.iscfunction(L, 1) ? 1 : 0);
    return 1;
}

// ── islclosure(fn) → boolean ──────────────────────────────────────────────────
static int luna_islclosure(lua_State* L) {
    if (!lua.isfunction(L, 1)) { lua.pushboolean(L, 0); return 1; }
    lua.pushboolean(L, (!lua.iscfunction(L, 1) && lua.isfunction(L, 1)) ? 1 : 0);
    return 1;
}

// ── clonefunction(fn) → function ──────────────────────────────────────────────
// Creates a copy of a function. For C closures, returns a new closure with the
// same underlying pointer. For L closures, creates a new closure over same proto.
static int luna_clonefunction(lua_State* L) {
    if (!lua.isfunction(L, 1)) {
        lua.pushstring(L, "clonefunction: expected function");
        lua.error(L);
    }

    RbxCClosure* cl = get_closure_ptr(L, 1);
    if (!cl) { lua.pushvalue(L, 1); return 1; }

    if (cl->isC) {
        // Wrap the same C function pointer in a new C closure
        lua_CFunction fn = cl->f;
        lua.pushcclosure(L, fn, "cloned_cclosure", 0);
    } else {
        // L-closure: push the same function as-is (deep clone requires proto duplication)
        // TODO: implement proper proto duplication for full UNC compliance
        lua.pushvalue(L, 1);
    }

    return 1;
}

// ── hookfunction(target, hook) → original ─────────────────────────────────────
// Replaces target's function pointer with hook, returns a newcclosure of original.
// Works for C closures. L-closure hooking requires bytecode patching (see debug_lib).
static int luna_hookfunction(lua_State* L) {
    if (!lua.isfunction(L, 1) || !lua.isfunction(L, 2)) {
        lua.pushstring(L, "hookfunction: expected (function, function)");
        lua.error(L);
    }

    RbxCClosure* target = get_closure_ptr(L, 1);
    if (!target || !target->isC) {
        lua.pushstring(L, "hookfunction: target must be a C closure");
        lua.error(L);
    }

    // Save original C function pointer
    lua_CFunction original_fn = target->f;

    // Get the hook function (must be a C closure for direct replacement)
    RbxCClosure* hook = get_closure_ptr(L, 2);
    if (!hook || !hook->isC) {
        lua.pushstring(L, "hookfunction: hook must be a C closure (wrap with newcclosure)");
        lua.error(L);
    }

    // Patch the function pointer (bypass page protection)
    memory::force_write<lua_CFunction>(
        reinterpret_cast<uintptr_t>(&target->f),
        hook->f
    );

    // Return a wrapper around the original function
    lua.pushcclosure(L, original_fn, "hooked_original", 0);
    return 1;
}

// ── getcallingscript() → Instance | nil ───────────────────────────────────────
// Returns the ModuleScript/LocalScript that owns the currently running thread.
// Stub — requires traversing the lua_State call stack for script identity.
static int luna_getcallingscript(lua_State* L) {
    // TODO: traverse L's call stack to find the script instance pointer
    lua.pushnil(L);
    return 1;
}

namespace environment {

void register_closures(lua_State* L) {
    lua.register_fn(L, "newcclosure",      luna_newcclosure);
    lua.register_fn(L, "iscclosure",       luna_iscclosure);
    lua.register_fn(L, "islclosure",       luna_islclosure);
    lua.register_fn(L, "clonefunction",    luna_clonefunction);
    lua.register_fn(L, "hookfunction",     luna_hookfunction);
    lua.register_fn(L, "getcallingscript", luna_getcallingscript);
}

} // namespace environment
