#include "hooks.h"
#include "../roblox/roblox.h"
#include "../environment/environment.h"
#include "../utils/memory.h"
#include "../../offsets.h"
#include <MinHook.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Hooks Implementation
// ─────────────────────────────────────────────────────────────────────────────

namespace hooks {

// ── Trampoline pointers (originals) ─────────────────────────────────────────
static void* o_script_scheduler = nullptr;
static bool  env_registered     = false;

// ── Script scheduler hook ─────────────────────────────────────────────────────
// We hook the function that fires when a new Luau script starts executing.
// This gives us the lua_State* of every script that runs, letting us inject
// our UNC environment before the script's code runs.
//
// The specific function to hook is version-dependent. Common targets:
//   - The job step function of "WaitingHybridScriptsJob"
//   - luaD_call in Luau's VM (but this fires very frequently — prefer the job)
//
// For this version we hook the task scheduler's job step via vtable patching.
// The job vtable layout for Roblox jobs:
//   [0] = destructor
//   [1] = step(double dt)
//   [2] = getName() → std::string&
//   ...

using JobStepFn = void(__thiscall*)(void* job, double dt);
static JobStepFn o_job_step = nullptr;

static void __fastcall hook_job_step(void* job, double dt) {
    // Call the original step first
    if (o_job_step) o_job_step(job, dt);

    // Inject environment once when we first get a valid lua_State
    if (!env_registered) {
        lua_State* L = roblox::get_script_state();
        if (L) {
            environment::register_all(L);
            env_registered = true;
        }
    }
}

bool initialize() {
    if (MH_Initialize() != MH_OK) return false;

    // ── Hook the WaitingHybridScriptsJob step ─────────────────────────────────
    // Find the job and grab its vtable step function pointer
    uintptr_t ts = roblox::get_task_scheduler();
    if (!ts) return false;

    uintptr_t job_start = memory::read<uintptr_t>(ts + offsets::TaskScheduler::JobStart);
    uintptr_t job_end   = memory::read<uintptr_t>(ts + offsets::TaskScheduler::JobEnd);

    for (uintptr_t cur = job_start; cur < job_end; cur += sizeof(uintptr_t)) {
        uintptr_t job = memory::read<uintptr_t>(cur);
        if (!job) continue;

        uintptr_t name_ptr = memory::read<uintptr_t>(job + offsets::TaskScheduler::JobName);
        if (!name_ptr) continue;
        std::string name = memory::read_string(name_ptr);

        if (name == "WaitingHybridScriptsJob") {
            // vtable[1] = step function
            uintptr_t vtable = memory::read<uintptr_t>(job);
            uintptr_t step_fn_ptr = vtable + sizeof(uintptr_t); // [1]
            uintptr_t step_fn = memory::read<uintptr_t>(step_fn_ptr);

            if (MH_CreateHook(
                reinterpret_cast<void*>(step_fn),
                reinterpret_cast<void*>(&hook_job_step),
                reinterpret_cast<void**>(&o_job_step)) == MH_OK)
            {
                MH_EnableHook(reinterpret_cast<void*>(step_fn));
            }
            break;
        }
    }

    return true;
}

void shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

int script_start_hook(lua_State* L, int nargs, int nresults) {
    if (!env_registered) {
        environment::register_all(L);
        env_registered = true;
    }
    return 0;
}

void error_hook(lua_State* /*L*/, const char* msg) {
    // Log to workspace/errors.log for debugging
    // (filesystem functions not available here — use direct write)
    if (!msg) return;
    HANDLE hFile = CreateFileA(
        "errors.log",
        FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &written, nullptr);
        WriteFile(hFile, "\r\n", 2, &written, nullptr);
        CloseHandle(hFile);
    }
}

} // namespace hooks
