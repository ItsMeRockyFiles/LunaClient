#include "../../lua/lua.h"
#include "../environment.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Metatable Functions
//  UNC spec: getrawmetatable, setrawmetatable, setreadonly, isreadonly,
//             makereadonly, makewriteable
// ─────────────────────────────────────────────────────────────────────────────

// Luau Table flags layout (from Luau source):
//   table->readonly is a uint8_t field at offset 0x18 in the Table struct
// Layout: GCObject* next, uint8_t tt, uint8_t marked, uint8_t memcat, uint8_t
// nodemask8
//         TValue* metatable (first field after GC header)
// We access via raw pointer arithmetic knowing the Luau Table struct layout.

// Luau Table struct (approximate, Roblox-flavored):
struct RbxTable {
  void *next;            // 0x00 GCObject next
  uint8_t tt;            // 0x08
  uint8_t marked;        // 0x09
  uint8_t memcat;        // 0x0A
  uint8_t readonly_flag; // 0x0B — the readonly bit we care about
                         // ... rest of table fields
};

// ── Internal: get Table* pointer from stack index
// ─────────────────────────────
static RbxTable *get_table_ptr(lua_State *L, int idx) {
  // lua_topointer returns a const void* to the TValue's GCObject
  return const_cast<RbxTable *>(
      static_cast<const RbxTable *>(lua.topointer(L, idx)));
}

// ── getrawmetatable(object) → table | nil
// ───────────────────────────────────── Bypasses __metatable by directly
// reading the GCObject's metatable field
static int luna_getrawmetatable(lua_State *L) {
  if (lua.top(L) < 1) {
    lua.pushnil(L);
    return 1;
  }

  // Use lua_getmetatable — but we need the raw access without __metatable
  // Approach: temporarily remove readonly, call getmetatable, restore
  // In practice, Luau's lua_getmetatable already ignores __metatable
  // restriction when called from C, so this works as expected.
  int has_mt = lua.getmetatable(L, 1);
  if (!has_mt)
    lua.pushnil(L);
  return 1;
}

// ── setrawmetatable(object, mt: table | nil)
// ──────────────────────────────────
static int luna_setrawmetatable(lua_State *L) {
  if (lua.top(L) < 2) {
    lua.pushstring(L, "setrawmetatable: expected 2 args");
    lua.error(L);
  }

  // If second arg is nil, remove metatable
  // Temporarily mark table as writable if it's readonly
  RbxTable *tbl = nullptr;
  bool was_readonly = false;

  if (lua.istable(L, 1)) {
    tbl = get_table_ptr(L, 1);
    if (tbl) {
      was_readonly = (tbl->readonly_flag & 0x1) != 0;
      tbl->readonly_flag &= ~0x1; // clear readonly bit
    }
  }

  lua.setmetatable(L, 1);

  // Restore readonly state
  if (tbl && was_readonly) {
    tbl->readonly_flag |= 0x1;
  }

  lua.pushvalue(L, 1);
  return 1;
}

// ── setreadonly(table, readonly: boolean)
// ─────────────────────────────────────
static int luna_setreadonly(lua_State *L) {
  if (!lua.istable(L, 1)) {
    lua.pushstring(L, "setreadonly: expected table");
    lua.error(L);
  }

  int readonly = lua.toboolean(L, 2);
  RbxTable *tbl = get_table_ptr(L, 1);
  if (tbl) {
    if (readonly)
      tbl->readonly_flag |= 0x1;
    else
      tbl->readonly_flag &= ~0x1;
  }
  return 0;
}

// ── isreadonly(table) → boolean
// ───────────────────────────────────────────────
static int luna_isreadonly(lua_State *L) {
  if (!lua.istable(L, 1)) {
    lua.pushboolean(L, 0);
    return 1;
  }
  RbxTable *tbl = get_table_ptr(L, 1);
  lua.pushboolean(L, tbl && (tbl->readonly_flag & 0x1) ? 1 : 0);
  return 1;
}

// ── makereadonly(table) → table  (convenience wrapper) ───────────────────────
static int luna_makereadonly(lua_State *L) {
  if (!lua.istable(L, 1)) {
    lua.pushstring(L, "makereadonly: expected table");
    lua.error(L);
  }
  RbxTable *tbl = get_table_ptr(L, 1);
  if (tbl)
    tbl->readonly_flag |= 0x1;
  lua.pushvalue(L, 1);
  return 1;
}

// ── makewriteable(table) → table  (convenience wrapper) ──────────────────────
static int luna_makewriteable(lua_State *L) {
  if (!lua.istable(L, 1)) {
    lua.pushstring(L, "makewriteable: expected table");
    lua.error(L);
  }
  RbxTable *tbl = get_table_ptr(L, 1);
  if (tbl)
    tbl->readonly_flag &= ~0x1;
  lua.pushvalue(L, 1);
  return 1;
}

namespace environment {

void register_metatables(lua_State *L) {
  lua.register_fn(L, "getrawmetatable", luna_getrawmetatable);
  lua.register_fn(L, "setrawmetatable", luna_setrawmetatable);
  lua.register_fn(L, "setreadonly", luna_setreadonly);
  lua.register_fn(L, "isreadonly", luna_isreadonly);
  lua.register_fn(L, "makereadonly", luna_makereadonly);
  lua.register_fn(L, "makewriteable", luna_makewriteable);
}

} // namespace environment
