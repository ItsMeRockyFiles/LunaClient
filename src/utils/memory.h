#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <string>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Memory utilities — safe read/write helpers for Roblox process memory
// ─────────────────────────────────────────────────────────────────────────────

namespace memory {

// Base address of RobloxPlayerBeta.exe in our process
inline uintptr_t base() {
    static uintptr_t cached = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return cached;
}

// Rebase an offset to an absolute address
inline uintptr_t rebase(uintptr_t offset) {
    return base() + offset;
}

// ── Safe read ────────────────────────────────────────────────────────────────
template<typename T>
inline T read(uintptr_t addr) {
    return *reinterpret_cast<T*>(addr);
}

template<typename T>
inline T read_offset(uintptr_t base_addr, uintptr_t offset) {
    return read<T>(base_addr + offset);
}

// ── Safe write ───────────────────────────────────────────────────────────────
template<typename T>
inline void write(uintptr_t addr, T value) {
    *reinterpret_cast<T*>(addr) = value;
}

template<typename T>
inline void write_offset(uintptr_t base_addr, uintptr_t offset, T value) {
    write<T>(base_addr + offset, value);
}

// ── Pointer chain read ───────────────────────────────────────────────────────
// e.g. ptr_chain(base, {0x10, 0x20, 0x8}) → [[base + 0x10] + 0x20] + 0x8
inline uintptr_t ptr_chain(uintptr_t addr, std::initializer_list<uintptr_t> offsets) {
    uintptr_t cur = addr;
    for (uintptr_t off : offsets) {
        if (!cur) return 0;
        cur = read<uintptr_t>(cur) + off;
    }
    return cur;
}

// ── Pattern scan ─────────────────────────────────────────────────────────────
// Scans .text section for a byte pattern with wildcards (0xFF = wildcard)
inline uintptr_t pattern_scan(const char* module_name, const uint8_t* pattern, const char* mask) {
    HMODULE hMod = module_name ? GetModuleHandleA(module_name) : GetModuleHandleA(nullptr);
    if (!hMod) return 0;

    auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
    auto nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(hMod) + dos->e_lfanew);

    uintptr_t text_start = reinterpret_cast<uintptr_t>(hMod) + nt->OptionalHeader.BaseOfCode;
    size_t    text_size  = nt->OptionalHeader.SizeOfCode;
    size_t    mask_len   = strlen(mask);

    for (size_t i = 0; i < text_size - mask_len; ++i) {
        bool found = true;
        for (size_t j = 0; j < mask_len; ++j) {
            if (mask[j] == 'x' && pattern[j] != *reinterpret_cast<uint8_t*>(text_start + i + j)) {
                found = false;
                break;
            }
        }
        if (found) return text_start + i;
    }
    return 0;
}

// ── Page protection helpers ──────────────────────────────────────────────────
inline DWORD protect(uintptr_t addr, size_t size, DWORD new_prot) {
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<void*>(addr), size, new_prot, &old);
    return old;
}

// Write bytes bypassing page protection
template<typename T>
inline void force_write(uintptr_t addr, T value) {
    DWORD old = protect(addr, sizeof(T), PAGE_EXECUTE_READWRITE);
    write<T>(addr, value);
    protect(addr, sizeof(T), old);
}

// ── String helpers ───────────────────────────────────────────────────────────
// Read a null-terminated string from an address (with safety limit)
inline std::string read_string(uintptr_t addr, size_t max_len = 512) {
    if (!addr) return {};
    std::string result;
    result.reserve(64);
    for (size_t i = 0; i < max_len; ++i) {
        char c = read<char>(addr + i);
        if (!c) break;
        result += c;
    }
    return result;
}

// Read a Roblox std::string (pointer + length stored inline at addr)
// Layout: [ptr: 8 bytes][len: 8 bytes] with SSO at 22 chars
inline std::string read_rbx_string(uintptr_t addr) {
    constexpr size_t SSO_LIMIT = 15;
    size_t len = read<size_t>(addr + 0x10);
    if (len == 0) return {};
    if (len <= SSO_LIMIT) {
        // Small string optimization — data stored inline
        return std::string(reinterpret_cast<const char*>(addr), len);
    }
    uintptr_t ptr = read<uintptr_t>(addr);
    if (!ptr) return {};
    return std::string(reinterpret_cast<const char*>(ptr), len);
}

} // namespace memory
