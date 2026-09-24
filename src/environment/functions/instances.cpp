#include "../environment.h"
#include "../../lua/lua.h"
#include "../../roblox/roblox.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Instance Functions
//  UNC spec: getinstances, getnilinstances, getscripts, getloadedmodules,
//             fireremoteevent, invokeclientevent, fireclickdetector,
//             fireproximityprompt, getconnections
// ─────────────────────────────────────────────────────────────────────────────

// ── Helper: recursively collect all instances from a root ─────────────────────
static void collect_instances(uintptr_t root, std::vector<uintptr_t>& out) {
    out.push_back(root);
    for (uintptr_t child : roblox::get_children(root)) {
        collect_instances(child, out);
    }
}

// ── getinstances() → table<Instance> ─────────────────────────────────────────
// Returns every descendant of DataModel (game)
static int luna_getinstances(lua_State* L) {
    uintptr_t dm = roblox::get_data_model();
    lua.createtable(L, 0, 0);
    if (!dm) return 1;

    std::vector<uintptr_t> instances;
    instances.reserve(1024);
    collect_instances(dm, instances);

    int i = 1;
    for (uintptr_t inst : instances) {
        lua.pushinteger(L, i++);
        // TODO: push actual Instance userdata — for now push as lightuserdata
        // Once Instance userdata binding is implemented, replace this
        lua.pushlightuserdata(L, reinterpret_cast<void*>(inst));
        lua.settable(L, -3);
    }
    return 1;
}

// ── getnilinstances() → table<Instance> ──────────────────────────────────────
// Returns instances whose Parent == nil (orphaned)
static int luna_getnilinstances(lua_State* L) {
    uintptr_t dm = roblox::get_data_model();
    lua.createtable(L, 0, 0);
    if (!dm) return 1;

    std::vector<uintptr_t> all;
    all.reserve(512);
    collect_instances(dm, all);

    int i = 1;
    for (uintptr_t inst : all) {
        uintptr_t parent = roblox::get_parent(inst);
        if (!parent) {
            lua.pushinteger(L, i++);
            lua.pushlightuserdata(L, reinterpret_cast<void*>(inst));
            lua.settable(L, -3);
        }
    }
    return 1;
}

// ── getscripts() → table<Script|LocalScript|ModuleScript> ────────────────────
static int luna_getscripts(lua_State* L) {
    uintptr_t dm = roblox::get_data_model();
    lua.createtable(L, 0, 0);
    if (!dm) return 1;

    std::vector<uintptr_t> all;
    all.reserve(512);
    collect_instances(dm, all);

    int i = 1;
    for (uintptr_t inst : all) {
        std::string cn = roblox::get_class_name(inst);
        if (cn == "Script" || cn == "LocalScript" || cn == "ModuleScript") {
            lua.pushinteger(L, i++);
            lua.pushlightuserdata(L, reinterpret_cast<void*>(inst));
            lua.settable(L, -3);
        }
    }
    return 1;
}

// ── getloadedmodules() → table<ModuleScript> ─────────────────────────────────
static int luna_getloadedmodules(lua_State* L) {
    uintptr_t dm = roblox::get_data_model();
    lua.createtable(L, 0, 0);
    if (!dm) return 1;

    std::vector<uintptr_t> all;
    all.reserve(256);
    collect_instances(dm, all);

    int i = 1;
    for (uintptr_t inst : all) {
        if (roblox::get_class_name(inst) == "ModuleScript") {
            lua.pushinteger(L, i++);
            lua.pushlightuserdata(L, reinterpret_cast<void*>(inst));
            lua.settable(L, -3);
        }
    }
    return 1;
}

// ── fireremoteevent(remote: RemoteEvent, ...) ─────────────────────────────────
// Calls FireServer on a RemoteEvent userdata
// UNC requires this to work on RemoteEvent instances
static int luna_fireremoteevent(lua_State* L) {
    // The FireServer function is at offsets::Functions::FireServer
    // Signature: FireServer(self: RemoteEvent, args...) — Roblox internal
    // We use the raw function pointer approach with the Luau call stack
    // TODO: implement full userdata binding to unwrap Instance pointer
    // For now, forward to Luau's __index chain if userdata is passed
    lua.pushstring(L, "fireremoteevent: Instance userdata binding required");
    lua.error(L);
    return 0;
}

// ── invokeclientevent(remote: RemoteFunction, player, ...) ───────────────────
static int luna_invokeclientevent(lua_State* L) {
    lua.pushstring(L, "invokeclientevent: Instance userdata binding required");
    lua.error(L);
    return 0;
}

// ── fireclickdetector(detector: ClickDetector, distance?: number) ─────────────
static int luna_fireclickdetector(lua_State* L) {
    // offsets::Fire::FireProximityPrompt gives us the vtable fn offset
    // TODO: implement after Instance userdata binding
    lua.pushstring(L, "fireclickdetector: Instance userdata binding required");
    lua.error(L);
    return 0;
}

// ── fireproximityprompt(prompt: ProximityPrompt) ──────────────────────────────
static int luna_fireproximityprompt(lua_State* L) {
    lua.pushstring(L, "fireproximityprompt: Instance userdata binding required");
    lua.error(L);
    return 0;
}

// ── getconnections(signal) → table ────────────────────────────────────────────
// Returns internal signal connections for a Roblox RBXScriptSignal
static int luna_getconnections(lua_State* L) {
    // TODO: walk the connection list for the signal userdata
    lua.createtable(L, 0, 0);
    return 1;
}

namespace environment {

void register_instances(lua_State* L) {
    lua.register_fn(L, "getinstances",       luna_getinstances);
    lua.register_fn(L, "getnilinstances",    luna_getnilinstances);
    lua.register_fn(L, "getscripts",         luna_getscripts);
    lua.register_fn(L, "getloadedmodules",   luna_getloadedmodules);
    lua.register_fn(L, "fireremoteevent",    luna_fireremoteevent);
    lua.register_fn(L, "invokeclientevent",  luna_invokeclientevent);
    lua.register_fn(L, "fireclickdetector",  luna_fireclickdetector);
    lua.register_fn(L, "fireproximityprompt",luna_fireproximityprompt);
    lua.register_fn(L, "getconnections",     luna_getconnections);
}

} // namespace environment
