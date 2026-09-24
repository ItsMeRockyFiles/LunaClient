#pragma once
#include "../lua/lua.h"
#include "../lua/types.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Environment — registers all UNC-standard functions onto the global table
//  UNC spec: https://github.com/unified-naming-convention/NamingStandard
// ─────────────────────────────────────────────────────────────────────────────

namespace environment {

// Push the entire UNC environment onto the given lua_State
void register_all(lua_State* L);

// ── Sub-libraries ─────────────────────────────────────────────────────────────
void register_filesystem(lua_State* L);  // readfile, writefile, appendfile, etc.
void register_closures(lua_State* L);    // hookfunction, newcclosure, etc.
void register_instances(lua_State* L);   // getinstances, getnilinstances, etc.
void register_metatables(lua_State* L);  // getrawmetatable, setreadonly, etc.
void register_debug(lua_State* L);       // debug.getconstants, etc.
void register_misc(lua_State* L);        // identifyexecutor, setfpscap, etc.
void register_http(lua_State* L);        // request / http.request

} // namespace environment
