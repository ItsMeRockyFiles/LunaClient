#pragma once
#include <cstdint>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  LunaClient shared named pipe protocol
//  Pipe name: \\.\pipe\LunaClient
//
//  Message format: [LunaHeader][payload bytes]
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char* LUNA_PIPE_NAME = "\\\\.\\pipe\\LunaClient";
constexpr uint32_t    LUNA_MAGIC     = 0x4C554E41; // "LUNA"
constexpr uint32_t    LUNA_PIPE_BUF  = 1024 * 512; // 512 KB pipe buffer

enum class LunaMsgType : uint32_t {
    Execute     = 1,  // EXE → DLL: execute a script
    Output      = 2,  // DLL → EXE: print/output from script
    Error       = 3,  // DLL → EXE: error from script
    Status      = 4,  // DLL → EXE: executor status update
    Ping        = 5,  // EXE → DLL: check if DLL is alive
    Pong        = 6,  // DLL → EXE: alive confirmation
};

#pragma pack(push, 1)
struct LunaHeader {
    uint32_t    magic;      // must be LUNA_MAGIC
    LunaMsgType type;
    uint32_t    payload_len; // bytes that follow this header

    static LunaHeader make(LunaMsgType t, uint32_t len) {
        return { LUNA_MAGIC, t, len };
    }

    bool valid() const { return magic == LUNA_MAGIC; }
};
#pragma pack(pop)

// Max script size we'll accept: 4 MB
constexpr uint32_t LUNA_MAX_SCRIPT_SIZE = 1024 * 1024 * 4;
