#pragma once
#include "types.h"
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Luau function pointer typedefs — resolved at runtime from Roblox's .text
//  All addresses are rebased to: GetModuleHandleA(nullptr) + offset
// ─────────────────────────────────────────────────────────────────────────────

// Stack manipulation
using fn_lua_gettop        = int          (*)(lua_State* L);
using fn_lua_settop        = void         (*)(lua_State* L, int idx);
using fn_lua_pushvalue     = void         (*)(lua_State* L, int idx);
using fn_lua_remove        = void         (*)(lua_State* L, int idx);
using fn_lua_insert        = void         (*)(lua_State* L, int idx);
using fn_lua_replace       = void         (*)(lua_State* L, int idx);
using fn_lua_checkstack    = int          (*)(lua_State* L, int sz);
using fn_lua_xmove         = void         (*)(lua_State* from, lua_State* to, int n);

// Access functions
using fn_lua_isnumber      = int          (*)(lua_State* L, int idx);
using fn_lua_isstring      = int          (*)(lua_State* L, int idx);
using fn_lua_iscfunction   = int          (*)(lua_State* L, int idx);
using fn_lua_isLfunction   = int          (*)(lua_State* L, int idx);
using fn_lua_isuserdata    = int          (*)(lua_State* L, int idx);
using fn_lua_type          = int          (*)(lua_State* L, int idx);
using fn_lua_typename      = const char*  (*)(lua_State* L, int tp);
using fn_lua_equal         = int          (*)(lua_State* L, int idx1, int idx2);
using fn_lua_rawequal      = int          (*)(lua_State* L, int idx1, int idx2);
using fn_lua_lessthan      = int          (*)(lua_State* L, int idx1, int idx2);

// Get functions
using fn_lua_tonumber      = lua_Number   (*)(lua_State* L, int idx);
using fn_lua_tointeger     = lua_Integer  (*)(lua_State* L, int idx);
using fn_lua_toboolean     = int          (*)(lua_State* L, int idx);
using fn_lua_tolstring     = const char*  (*)(lua_State* L, int idx, size_t* len);
using fn_lua_objlen        = size_t       (*)(lua_State* L, int idx);
using fn_lua_tocfunction   = lua_CFunction(*)(lua_State* L, int idx);
using fn_lua_touserdata    = void*        (*)(lua_State* L, int idx);
using fn_lua_tothread      = lua_State*   (*)(lua_State* L, int idx);
using fn_lua_topointer     = const void*  (*)(lua_State* L, int idx);

// Push functions
using fn_lua_pushnil       = void         (*)(lua_State* L);
using fn_lua_pushnumber    = void         (*)(lua_State* L, lua_Number n);
using fn_lua_pushinteger   = void         (*)(lua_State* L, lua_Integer n);
using fn_lua_pushunsigned  = void         (*)(lua_State* L, lua_Unsigned n);
using fn_lua_pushlstring   = void         (*)(lua_State* L, const char* s, size_t l);
using fn_lua_pushstring    = void         (*)(lua_State* L, const char* s);
using fn_lua_pushboolean   = void         (*)(lua_State* L, int b);
using fn_lua_pushcclosure  = void         (*)(lua_State* L, lua_CFunction fn, const char* debugname, int nup);
using fn_lua_pushlightuserdata = void     (*)(lua_State* L, void* p);
using fn_lua_pushthread    = int          (*)(lua_State* L);

// Get/Set table
using fn_lua_gettable      = void         (*)(lua_State* L, int idx);
using fn_lua_getfield      = void         (*)(lua_State* L, int idx, const char* k);
using fn_lua_rawget        = void         (*)(lua_State* L, int idx);
using fn_lua_rawgeti       = void         (*)(lua_State* L, int idx, int n);
using fn_lua_createtable   = void         (*)(lua_State* L, int narr, int nrec);
using fn_lua_newuserdata   = void*        (*)(lua_State* L, size_t sz, int tag);
using fn_lua_getmetatable  = int          (*)(lua_State* L, int objindex);
using fn_lua_getfenv       = void         (*)(lua_State* L, int idx);
using fn_lua_settable      = void         (*)(lua_State* L, int idx);
using fn_lua_setfield      = void         (*)(lua_State* L, int idx, const char* k);
using fn_lua_rawset        = void         (*)(lua_State* L, int idx);
using fn_lua_rawseti       = void         (*)(lua_State* L, int idx, int n);
using fn_lua_setmetatable  = int          (*)(lua_State* L, int objindex);
using fn_lua_setfenv       = int          (*)(lua_State* L, int idx);

// Call / load
using fn_lua_call          = void         (*)(lua_State* L, int nargs, int nresults);
using fn_lua_pcall         = int          (*)(lua_State* L, int nargs, int nresults, int errfunc);
using fn_lua_cpcall        = int          (*)(lua_State* L, lua_CFunction func, void* ud);
using fn_luaL_loadbuffer   = int          (*)(lua_State* L, const char* buff, size_t sz, const char* name);
using fn_luaL_loadstring   = int          (*)(lua_State* L, const char* s);

// Misc
using fn_lua_error         = int          (*)(lua_State* L);
using fn_lua_next          = int          (*)(lua_State* L, int idx);
using fn_lua_concat        = void         (*)(lua_State* L, int n);
using fn_lua_gc            = int          (*)(lua_State* L, int what, int data);
using fn_lua_newstate      = lua_State*   (*)(lua_Alloc f, void* ud);
using fn_lua_close         = void         (*)(lua_State* L);
using fn_lua_newthread     = lua_State*   (*)(lua_State* L);
using fn_lua_atpanic       = lua_CFunction(*)(lua_State* L, lua_CFunction panicf);
using fn_lua_ref           = int          (*)(lua_State* L, int t);
using fn_lua_unref         = void         (*)(lua_State* L, int t, int ref);
using fn_luaL_ref         = int          (*)(lua_State* L, int t);
using fn_luaL_unref       = void         (*)(lua_State* L, int t, int ref);

// ─────────────────────────────────────────────────────────────────────────────
//  lua_api — global singleton holding all resolved Lua function pointers
// ─────────────────────────────────────────────────────────────────────────────
struct lua_api {
    // Stack
    fn_lua_gettop        gettop        = nullptr;
    fn_lua_settop        settop        = nullptr;
    fn_lua_pushvalue     pushvalue     = nullptr;
    fn_lua_remove        remove        = nullptr;
    fn_lua_insert        insert        = nullptr;
    fn_lua_replace       replace       = nullptr;
    fn_lua_checkstack    checkstack    = nullptr;

    // Type checks
    fn_lua_isnumber      isnumber      = nullptr;
    fn_lua_isstring      isstring      = nullptr;
    fn_lua_iscfunction   iscfunction   = nullptr;
    fn_lua_isLfunction   isLfunction   = nullptr;
    fn_lua_isuserdata    isuserdata    = nullptr;
    fn_lua_type          type          = nullptr;
    fn_lua_typename      typename_     = nullptr;

    // Getters
    fn_lua_tonumber      tonumber      = nullptr;
    fn_lua_tointeger     tointeger     = nullptr;
    fn_lua_toboolean     toboolean     = nullptr;
    fn_lua_tolstring     tolstring     = nullptr;
    fn_lua_objlen        objlen        = nullptr;
    fn_lua_tocfunction   tocfunction   = nullptr;
    fn_lua_touserdata    touserdata    = nullptr;
    fn_lua_tothread      tothread      = nullptr;
    fn_lua_topointer     topointer     = nullptr;

    // Pushers
    fn_lua_pushnil       pushnil       = nullptr;
    fn_lua_pushnumber    pushnumber    = nullptr;
    fn_lua_pushinteger   pushinteger   = nullptr;
    fn_lua_pushlstring   pushlstring   = nullptr;
    fn_lua_pushstring    pushstring    = nullptr;
    fn_lua_pushboolean   pushboolean   = nullptr;
    fn_lua_pushcclosure  pushcclosure  = nullptr;
    fn_lua_pushlightuserdata pushlightuserdata = nullptr;

    // Tables
    fn_lua_gettable      gettable      = nullptr;
    fn_lua_getfield      getfield      = nullptr;
    fn_lua_rawget        rawget        = nullptr;
    fn_lua_rawgeti       rawgeti       = nullptr;
    fn_lua_createtable   createtable   = nullptr;
    fn_lua_newuserdata   newuserdata   = nullptr;
    fn_lua_getmetatable  getmetatable  = nullptr;
    fn_lua_settable      settable      = nullptr;
    fn_lua_setfield      setfield      = nullptr;
    fn_lua_rawset        rawset        = nullptr;
    fn_lua_rawseti       rawseti       = nullptr;
    fn_lua_setmetatable  setmetatable  = nullptr;

    // Call/load
    fn_lua_call          call          = nullptr;
    fn_lua_pcall         pcall         = nullptr;
    fn_luaL_loadbuffer   loadbuffer    = nullptr;
    fn_luaL_loadstring   loadstring    = nullptr;

    // Misc
    fn_lua_error         error         = nullptr;
    fn_lua_next          next          = nullptr;
    fn_lua_concat        concat        = nullptr;
    fn_lua_gc            gc            = nullptr;
    fn_lua_newthread     newthread     = nullptr;
    fn_luaL_ref          ref           = nullptr;
    fn_luaL_unref        unref         = nullptr;

    // ── Convenience wrappers ─────────────────────────────────────────────────
    inline int  top(lua_State* L)                              { return gettop(L); }
    inline void pop(lua_State* L, int n)                       { settop(L, -(n)-1); }
    inline bool isnil(lua_State* L, int idx)                   { return type(L, idx) == LUA_TNIL; }
    inline bool isboolean(lua_State* L, int idx)               { return type(L, idx) == LUA_TBOOLEAN; }
    inline bool istable(lua_State* L, int idx)                 { return type(L, idx) == LUA_TTABLE; }
    inline bool isfunction(lua_State* L, int idx)              { return type(L, idx) == LUA_TFUNCTION; }
    inline bool isthread(lua_State* L, int idx)                { return type(L, idx) == LUA_TTHREAD; }
    inline const char* tostring(lua_State* L, int idx)        { return tolstring(L, idx, nullptr); }
    inline void newtable(lua_State* L)                         { createtable(L, 0, 0); }
    inline void setglobal(lua_State* L, const char* name)     { setfield(L, LUA_GLOBALSINDEX, name); }
    inline void getglobal(lua_State* L, const char* name)     { getfield(L, LUA_GLOBALSINDEX, name); }

    // Register a C function as a global
    inline void register_fn(lua_State* L, const char* name, lua_CFunction fn) {
        pushcclosure(L, fn, name, 0);
        setglobal(L, name);
    }
};

// Global lua API instance — initialized in roblox.cpp
extern lua_api lua;
