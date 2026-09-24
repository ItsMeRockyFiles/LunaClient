#include "pipe_server.h"
#include <vector>
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
//  Pipe server implementation
// ─────────────────────────────────────────────────────────────────────────────

namespace pipe_server {

static std::thread       g_thread;
static std::atomic<bool> g_running   { false };
static HANDLE            g_pipe      { INVALID_HANDLE_VALUE };
static std::mutex        g_write_mtx;
static ExecuteCallback   g_on_execute;
static StatusCallback    g_on_status;

// ── Helper: write a full message ──────────────────────────────────────────────
static bool write_msg(HANDLE pipe, LunaMsgType type, const std::string& payload) {
    if (pipe == INVALID_HANDLE_VALUE || pipe == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_write_mtx);

    LunaHeader hdr = LunaHeader::make(type, static_cast<uint32_t>(payload.size()));
    DWORD written = 0;
    if (!WriteFile(pipe, &hdr, sizeof(hdr), &written, nullptr)) return false;
    if (!payload.empty()) {
        if (!WriteFile(pipe, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)) return false;
    }
    return true;
}

// ── Helper: read a full message ───────────────────────────────────────────────
static bool read_msg(HANDLE pipe, LunaHeader& hdr_out, std::string& payload_out) {
    DWORD read = 0;
    if (!ReadFile(pipe, &hdr_out, sizeof(LunaHeader), &read, nullptr) || read != sizeof(LunaHeader))
        return false;
    if (!hdr_out.valid()) return false;

    if (hdr_out.payload_len == 0) return true;
    if (hdr_out.payload_len > LUNA_MAX_SCRIPT_SIZE) return false;

    payload_out.resize(hdr_out.payload_len);
    DWORD total = 0;
    while (total < hdr_out.payload_len) {
        if (!ReadFile(pipe, &payload_out[total], hdr_out.payload_len - total, &read, nullptr))
            return false;
        total += read;
    }
    return true;
}

// ── Server thread ─────────────────────────────────────────────────────────────
static void server_loop() {
    while (g_running) {
        // Create a named pipe instance each time (reconnect after disconnect)
        HANDLE pipe = CreateNamedPipeA(
            LUNA_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,                  // max instances
            LUNA_PIPE_BUF,      // out buffer
            LUNA_PIPE_BUF,      // in buffer
            5000,               // default timeout ms
            nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }

        g_pipe = pipe;

        // Wait for client connection (blocks)
        if (!ConnectNamedPipe(pipe, nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                g_pipe = INVALID_HANDLE_VALUE;
                continue;
            }
        }

        // Send status on connect
        if (g_on_status) {
            write_msg(pipe, LunaMsgType::Status, g_on_status());
        }

        // Message loop
        while (g_running) {
            LunaHeader hdr {};
            std::string payload;

            if (!read_msg(pipe, hdr, payload)) break; // client disconnected

            switch (hdr.type) {
                case LunaMsgType::Execute:
                    if (g_on_execute) g_on_execute(payload);
                    break;

                case LunaMsgType::Ping:
                    write_msg(pipe, LunaMsgType::Pong, "alive");
                    break;

                default:
                    break;
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void start(ExecuteCallback on_execute, StatusCallback on_status) {
    if (g_running) return;
    g_on_execute = std::move(on_execute);
    g_on_status  = std::move(on_status);
    g_running    = true;
    g_thread     = std::thread(server_loop);
    g_thread.detach();
}

void stop() {
    g_running = false;
    // Force the blocking ConnectNamedPipe to return by opening and closing a client
    HANDLE tmp = CreateFileA(LUNA_PIPE_NAME, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (tmp != INVALID_HANDLE_VALUE) CloseHandle(tmp);
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(g_pipe, nullptr);
    }
}

void send_output(const std::string& text) {
    if (g_pipe != INVALID_HANDLE_VALUE)
        write_msg(g_pipe, LunaMsgType::Output, text);
}

void send_error(const std::string& text) {
    if (g_pipe != INVALID_HANDLE_VALUE)
        write_msg(g_pipe, LunaMsgType::Error, text);
}

} // namespace pipe_server
