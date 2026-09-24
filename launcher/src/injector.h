#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  DLL Injector — classic LoadLibraryA remote thread injection
// ─────────────────────────────────────────────────────────────────────────────

namespace injector {

enum class InjectResult {
    Success,
    ProcessNotFound,
    OpenProcessFailed,
    AllocFailed,
    WriteFailed,
    ThreadFailed,
    AlreadyInjected,
};

struct InjectStatus {
    InjectResult result;
    DWORD        pid;
    std::string  message;
};

// Find Roblox process ID (RobloxPlayerBeta.exe)
DWORD find_roblox_pid();

// Check if our DLL is already loaded in Roblox
bool is_injected(DWORD pid, const std::string& dll_name);

// Inject dll_path into process with given pid
InjectStatus inject(DWORD pid, const std::string& dll_path);

// Convenience: auto-find Roblox and inject
InjectStatus inject_into_roblox(const std::string& dll_path);

// Human-readable result string
std::string result_to_string(InjectResult r);

} // namespace injector
