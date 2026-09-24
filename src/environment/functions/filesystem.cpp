#include "../environment.h"
#include "../../lua/lua.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  UNC Filesystem Functions
//  All file operations are sandboxed to %localappdata%\LunaClient\workspace\
//  UNC spec: readfile, writefile, appendfile, isfile, isfolder, makefolder,
//             listfiles, delfile, delfolder, loadfile, dofile
// ─────────────────────────────────────────────────────────────────────────────

namespace fs = std::filesystem;

// ── Sandbox root ─────────────────────────────────────────────────────────────
static std::string get_workspace_root() {
    static std::string root;
    if (!root.empty()) return root;

    char appdata[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata);
    root = std::string(appdata) + "\\LunaClient\\workspace\\";

    // Ensure it exists
    fs::create_directories(root);
    return root;
}

// Resolve a user-provided path against the sandbox root.
// Returns empty string if the resolved path escapes the sandbox.
static std::string resolve_path(const std::string& rel) {
    fs::path base = fs::path(get_workspace_root());
    fs::path resolved = fs::weakly_canonical(base / rel);

    // Security: make sure resolved path starts with sandbox root
    std::string resolved_str = resolved.string();
    std::string base_str     = fs::canonical(base).string();
    if (resolved_str.rfind(base_str, 0) != 0)
        return {}; // path escape attempt

    return resolved_str;
}

// ── readfile(path: string) → string ──────────────────────────────────────────
static int luna_readfile(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushstring(L, "readfile: expected string"); lua.error(L); }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "readfile: invalid path"); lua.error(L); }

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { lua.pushstring(L, "readfile: file not found"); lua.error(L); }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    lua.pushlstring(L, content.data(), content.size());
    return 1;
}

// ── writefile(path: string, content: string) ──────────────────────────────────
static int luna_writefile(lua_State* L) {
    if (!lua.isstring(L, 1) || !lua.isstring(L, 2)) {
        lua.pushstring(L, "writefile: expected (string, string)"); lua.error(L);
    }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "writefile: invalid path"); lua.error(L); }

    size_t len = 0;
    const char* data = lua.tolstring(L, 2, &len);

    // Create parent directories if needed
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) { lua.pushstring(L, "writefile: could not open file"); lua.error(L); }
    f.write(data, static_cast<std::streamsize>(len));
    return 0;
}

// ── appendfile(path: string, content: string) ────────────────────────────────
static int luna_appendfile(lua_State* L) {
    if (!lua.isstring(L, 1) || !lua.isstring(L, 2)) {
        lua.pushstring(L, "appendfile: expected (string, string)"); lua.error(L);
    }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "appendfile: invalid path"); lua.error(L); }

    size_t len = 0;
    const char* data = lua.tolstring(L, 2, &len);

    fs::create_directories(fs::path(path).parent_path());

    std::ofstream f(path, std::ios::binary | std::ios::app);
    if (!f.is_open()) { lua.pushstring(L, "appendfile: could not open file"); lua.error(L); }
    f.write(data, static_cast<std::streamsize>(len));
    return 0;
}

// ── isfile(path: string) → boolean ───────────────────────────────────────────
static int luna_isfile(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushboolean(L, 0); return 1; }
    std::string path = resolve_path(lua.tostring(L, 1));
    lua.pushboolean(L, !path.empty() && fs::is_regular_file(path) ? 1 : 0);
    return 1;
}

// ── isfolder(path: string) → boolean ─────────────────────────────────────────
static int luna_isfolder(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushboolean(L, 0); return 1; }
    std::string path = resolve_path(lua.tostring(L, 1));
    lua.pushboolean(L, !path.empty() && fs::is_directory(path) ? 1 : 0);
    return 1;
}

// ── makefolder(path: string) ──────────────────────────────────────────────────
static int luna_makefolder(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushstring(L, "makefolder: expected string"); lua.error(L); }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "makefolder: invalid path"); lua.error(L); }
    fs::create_directories(path);
    return 0;
}

// ── delfolder(path: string) ───────────────────────────────────────────────────
static int luna_delfolder(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushstring(L, "delfolder: expected string"); lua.error(L); }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "delfolder: invalid path"); lua.error(L); }
    if (fs::is_directory(path)) fs::remove_all(path);
    return 0;
}

// ── delfile(path: string) ─────────────────────────────────────────────────────
static int luna_delfile(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushstring(L, "delfile: expected string"); lua.error(L); }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushstring(L, "delfile: invalid path"); lua.error(L); }
    fs::remove(path);
    return 0;
}

// ── listfiles(path: string) → table<string> ──────────────────────────────────
static int luna_listfiles(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushstring(L, "listfiles: expected string"); lua.error(L); }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty() || !fs::is_directory(path)) {
        lua.createtable(L, 0, 0);
        return 1;
    }

    lua.createtable(L, 0, 0);
    int i = 1;
    for (auto& entry : fs::directory_iterator(path)) {
        std::string entry_path = entry.path().string();
        // Return relative paths within workspace
        std::string rel = entry_path.substr(get_workspace_root().size());
        lua.pushinteger(L, i++);
        lua.pushstring(L, rel.c_str());
        lua.settable(L, -3);
    }
    return 1;
}

// ── loadfile(path: string) → function | nil, err ─────────────────────────────
static int luna_loadfile(lua_State* L) {
    if (!lua.isstring(L, 1)) { lua.pushnil(L); lua.pushstring(L, "loadfile: expected string"); return 2; }
    std::string path = resolve_path(lua.tostring(L, 1));
    if (path.empty()) { lua.pushnil(L); lua.pushstring(L, "loadfile: invalid path"); return 2; }

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { lua.pushnil(L); lua.pushstring(L, "loadfile: file not found"); return 2; }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    int result = lua.loadbuffer(L, src.data(), src.size(), ("@" + path).c_str());
    if (result != LUA_OK) {
        lua.pushnil(L);
        lua.insert(L, -2); // move error msg to pos 2
        return 2;
    }
    return 1; // function on stack
}

// ── dofile(path: string) → ... ────────────────────────────────────────────────
static int luna_dofile(lua_State* L) {
    int load_results = luna_loadfile(L);
    if (load_results == 2) return 2; // propagate nil, err

    // Call the loaded chunk
    int top_before = lua.top(L) - 1; // subtract the function itself
    int call_result = lua.pcall(L, 0, -1, 0);
    if (call_result != LUA_OK) {
        lua.error(L);
        return 0;
    }
    return lua.top(L) - top_before;
}

namespace environment {

void register_filesystem(lua_State* L) {
    lua.register_fn(L, "readfile",   luna_readfile);
    lua.register_fn(L, "writefile",  luna_writefile);
    lua.register_fn(L, "appendfile", luna_appendfile);
    lua.register_fn(L, "isfile",     luna_isfile);
    lua.register_fn(L, "isfolder",   luna_isfolder);
    lua.register_fn(L, "makefolder", luna_makefolder);
    lua.register_fn(L, "delfolder",  luna_delfolder);
    lua.register_fn(L, "delfile",    luna_delfile);
    lua.register_fn(L, "listfiles",  luna_listfiles);
    lua.register_fn(L, "loadfile",   luna_loadfile);
    lua.register_fn(L, "dofile",     luna_dofile);
}

} // namespace environment
