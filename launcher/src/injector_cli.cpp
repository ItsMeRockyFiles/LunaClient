// ─────────────────────────────────────────────────────────────────────────────
//  LunaInjector CLI — standalone injector spawned by the Node.js backend
//  Usage: LunaInjector.exe [dll_path]
//  Output: JSON status to stdout for the backend to parse
// ─────────────────────────────────────────────────────────────────────────────

#include "injector.h"
#include <cstdio>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::string dll_path;

    if (argc >= 2) {
        dll_path = argv[1];
    } else {
        // Default: look for LunaClient.dll next to this exe
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        dll_path = (std::filesystem::path(exe_path).parent_path() / "LunaClient.dll").string();
    }

    if (!std::filesystem::exists(dll_path)) {
        printf("{\"success\":false,\"error\":\"DLL not found: %s\"}\n", dll_path.c_str());
        return 1;
    }

    auto status = injector::inject_into_roblox(dll_path);

    bool ok = (status.result == injector::InjectResult::Success ||
               status.result == injector::InjectResult::AlreadyInjected);

    // Escape quotes in message for JSON
    std::string msg = status.message;
    for (size_t i = 0; i < msg.size(); ++i) {
        if (msg[i] == '"') { msg.insert(i, "\\"); ++i; }
        if (msg[i] == '\\' && (i + 1 >= msg.size() || msg[i+1] != '"')) {
            msg.insert(i, "\\"); ++i;
        }
    }

    printf("{\"success\":%s,\"pid\":%lu,\"result\":\"%s\",\"message\":\"%s\"}\n",
        ok ? "true" : "false",
        status.pid,
        injector::result_to_string(status.result).c_str(),
        msg.c_str());

    return ok ? 0 : 1;
}
