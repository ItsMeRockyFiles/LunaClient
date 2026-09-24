#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <sstream>

#include "roblox/roblox.h"
#include "environment/environment.h"
#include "hooks/hooks.h"
#include "pipe/pipe_server.h"
#include "../offsets.h"
#include "utils/memory.h"

// ─────────────────────────────────────────────────────────────────────────────
//  LunaClient — DLL Entry Point
//
//  Injection flow:
//    1. DLL injected into RobloxPlayerBeta.exe by LunaLauncher.exe
//    2. DllMain spawns main_thread
//    3. main_thread waits for DataModel to be ready
//    4. Resolves all Luau function pointers via pattern scan
//    5. Registers UNC environment onto the script lua_State
//    6. Installs hooks (job step hook for persistent env injection)
//    7. Starts named pipe server — listens for scripts from the launcher
// ─────────────────────────────────────────────────────────────────────────────

// Global script state — updated each time the hook fires
static lua_State* g_L = nullptr;

// ── Script executor ───────────────────────────────────────────────────────────
// Called by the pipe server when the launcher sends a script
static void execute_script(const std::string& script) {
    lua_State* L = g_L;
    if (!L) {
        pipe_server::send_error("[LunaClient] Not attached — no lua_State available.");
        return;
    }

    // Hook print() to forward output back through the pipe
    // We temporarily replace print with our pipe forwarder
    lua.register_fn(L, "print", [](lua_State* LL) -> int {
        int n = lua.top(LL);
        std::ostringstream ss;
        for (int i = 1; i <= n; ++i) {
            if (i > 1) ss << "\t";
            const char* s = lua.tostring(LL, i);
            ss << (s ? s : "nil");
        }
        pipe_server::send_output(ss.str());
        return 0;
    });

    // Load the script
    int load_result = lua.loadbuffer(L, script.data(), script.size(), "LunaScript");
    if (load_result != LUA_OK) {
        const char* err = lua.tostring(L, -1);
        pipe_server::send_error(std::string("[Syntax Error] ") + (err ? err : "unknown"));
        lua.pop(L, 1);
        return;
    }

    // Execute
    int exec_result = lua.pcall(L, 0, 0, 0);
    if (exec_result != LUA_OK) {
        const char* err = lua.tostring(L, -1);
        pipe_server::send_error(std::string("[Runtime Error] ") + (err ? err : "unknown"));
        lua.pop(L, 1);
    }
}

// ── Status callback ───────────────────────────────────────────────────────────
static std::string get_status() {
    std::ostringstream ss;
    ss << "LunaClient v" LUNA_VERSION " attached\n";
    ss << "PlaceId: " << roblox::get_place_id() << "\n";
    ss << "GameId: "  << roblox::get_game_id()  << "\n";
    ss << "JobId: "   << roblox::get_job_id()   << "\n";
    ss << "Lua state: " << (g_L ? "ready" : "pending") << "\n";
    return ss.str();
}

// ── Main injection thread ─────────────────────────────────────────────────────
static void main_thread(HMODULE hModule) {
    constexpr DWORD POLL_MS  = 500;
    constexpr DWORD MAX_WAIT = 60000;
    DWORD elapsed = 0;

    // ── Wait for DataModel ────────────────────────────────────────────────────
    while (elapsed < MAX_WAIT) {
        uintptr_t dm = roblox::get_data_model();
        if (dm && memory::read<bool>(dm + offsets::DataModel::GameLoaded)) break;
        Sleep(POLL_MS);
        elapsed += POLL_MS;
    }

    // ── Find script lua_State ─────────────────────────────────────────────────
    elapsed = 0;
    while (elapsed < MAX_WAIT) {
        g_L = roblox::get_script_state();
        if (g_L) break;
        Sleep(POLL_MS);
        elapsed += POLL_MS;
    }

    if (!g_L) {
        FreeLibraryAndExitThread(hModule, 1);
        return;
    }

    // ── Initialize Luau API ───────────────────────────────────────────────────
    if (!roblox::initialize(g_L)) {
        FreeLibraryAndExitThread(hModule, 1);
        return;
    }

    // ── Register UNC environment ──────────────────────────────────────────────
    environment::register_all(g_L);

    // ── Install hooks ─────────────────────────────────────────────────────────
    hooks::initialize();

    // ── Start pipe server ─────────────────────────────────────────────────────
    pipe_server::start(execute_script, get_status);

    // ── Keep alive (eject on END) ─────────────────────────────────────────────
    while (true) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            pipe_server::stop();
            hooks::shutdown();
            FreeLibraryAndExitThread(hModule, 0);
            return;
        }
        // Keep g_L fresh — the hook updates it, but also poll here as fallback
        lua_State* fresh = roblox::get_script_state();
        if (fresh) g_L = fresh;
        Sleep(200);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(
            nullptr, 0,
            [](LPVOID param) -> DWORD {
                main_thread(static_cast<HMODULE>(param));
                return 0;
            },
            hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
