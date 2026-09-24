#include "injector.h"
#include <Psapi.h>
#include <filesystem>
#include <sstream>

#pragma comment(lib, "Psapi.lib")

namespace injector {

// ── Find Roblox PID ───────────────────────────────────────────────────────────
DWORD find_roblox_pid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            // Match RobloxPlayerBeta.exe (also handle RobloxPlayer.exe for older installs)
            std::wstring name = pe.szExeFile;
            if (name == L"RobloxPlayerBeta.exe" || name == L"RobloxPlayer.exe") {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// ── Check if DLL is already injected ─────────────────────────────────────────
bool is_injected(DWORD pid, const std::string& dll_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me = {};
    me.dwSize = sizeof(me);

    std::wstring target(dll_name.begin(), dll_name.end());
    bool found = false;

    if (Module32FirstW(snap, &me)) {
        do {
            std::wstring mod_name = me.szModule;
            if (mod_name == target) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

// ── Core injection ────────────────────────────────────────────────────────────
InjectStatus inject(DWORD pid, const std::string& dll_path) {
    InjectStatus status;
    status.pid = pid;

    if (!pid) {
        status.result  = InjectResult::ProcessNotFound;
        status.message = "Roblox process not found. Open Roblox first.";
        return status;
    }

    // Resolve absolute path
    std::filesystem::path abs_path = std::filesystem::absolute(dll_path);
    std::string abs_str = abs_path.string();

    // Check already injected
    if (is_injected(pid, abs_path.filename().string())) {
        status.result  = InjectResult::AlreadyInjected;
        status.message = "LunaClient.dll is already injected.";
        return status;
    }

    // Open process with required access
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, pid);

    if (!hProcess) {
        status.result  = InjectResult::OpenProcessFailed;
        status.message = "OpenProcess failed. Try running as Administrator. Error: "
                       + std::to_string(GetLastError());
        return status;
    }

    // Allocate space for DLL path string in remote process
    size_t path_len = abs_str.size() + 1;
    LPVOID remote_mem = VirtualAllocEx(hProcess, nullptr, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_mem) {
        CloseHandle(hProcess);
        status.result  = InjectResult::AllocFailed;
        status.message = "VirtualAllocEx failed. Error: " + std::to_string(GetLastError());
        return status;
    }

    // Write DLL path
    if (!WriteProcessMemory(hProcess, remote_mem, abs_str.c_str(), path_len, nullptr)) {
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        status.result  = InjectResult::WriteFailed;
        status.message = "WriteProcessMemory failed. Error: " + std::to_string(GetLastError());
        return status;
    }

    // Resolve LoadLibraryA address (same in all processes on same OS — ASLR shared)
    LPVOID load_library = reinterpret_cast<LPVOID>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));

    // Create remote thread calling LoadLibraryA(dll_path)
    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(load_library),
        remote_mem, 0, nullptr);

    if (!hThread) {
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        status.result  = InjectResult::ThreadFailed;
        status.message = "CreateRemoteThread failed. Error: " + std::to_string(GetLastError());
        return status;
    }

    // Wait for injection to complete (up to 10s)
    WaitForSingleObject(hThread, 10000);

    // Cleanup
    VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    status.result  = InjectResult::Success;
    status.message = "Injected successfully into PID " + std::to_string(pid);
    return status;
}

InjectStatus inject_into_roblox(const std::string& dll_path) {
    return inject(find_roblox_pid(), dll_path);
}

std::string result_to_string(InjectResult r) {
    switch (r) {
        case InjectResult::Success:          return "Success";
        case InjectResult::ProcessNotFound:  return "Process Not Found";
        case InjectResult::OpenProcessFailed:return "Access Denied";
        case InjectResult::AllocFailed:      return "Memory Allocation Failed";
        case InjectResult::WriteFailed:      return "Memory Write Failed";
        case InjectResult::ThreadFailed:     return "Remote Thread Failed";
        case InjectResult::AlreadyInjected:  return "Already Injected";
        default:                             return "Unknown";
    }
}

} // namespace injector
