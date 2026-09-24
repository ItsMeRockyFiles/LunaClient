#include "pipe_client.h"

namespace pipe_client {

static HANDLE            g_pipe   { INVALID_HANDLE_VALUE };
static std::thread       g_thread;
static std::atomic<bool> g_running { false };
static std::atomic<ConnState> g_state { ConnState::Disconnected };
static OnMessageCallback g_on_msg;
static std::mutex        g_write_mtx;

// ── Read helpers ──────────────────────────────────────────────────────────────
static bool read_exact(HANDLE pipe, void* buf, DWORD len) {
    DWORD total = 0;
    while (total < len) {
        DWORD read = 0;
        if (!ReadFile(pipe, static_cast<uint8_t*>(buf) + total, len - total, &read, nullptr) || read == 0)
            return false;
        total += read;
    }
    return true;
}

static bool write_msg(HANDLE pipe, LunaMsgType type, const std::string& payload) {
    std::lock_guard<std::mutex> lock(g_write_mtx);
    LunaHeader hdr = LunaHeader::make(type, static_cast<uint32_t>(payload.size()));
    DWORD written = 0;
    if (!WriteFile(pipe, &hdr, sizeof(hdr), &written, nullptr)) return false;
    if (!payload.empty()) {
        if (!WriteFile(pipe, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)) return false;
    }
    return true;
}

// ── Reader thread ─────────────────────────────────────────────────────────────
static void reader_loop() {
    while (g_running && g_pipe != INVALID_HANDLE_VALUE) {
        LunaHeader hdr {};
        if (!read_exact(g_pipe, &hdr, sizeof(hdr)) || !hdr.valid()) {
            g_state = ConnState::Disconnected;
            break;
        }

        std::string payload;
        if (hdr.payload_len > 0 && hdr.payload_len <= LUNA_MAX_SCRIPT_SIZE) {
            payload.resize(hdr.payload_len);
            if (!read_exact(g_pipe, &payload[0], hdr.payload_len)) {
                g_state = ConnState::Disconnected;
                break;
            }
        }

        if (g_on_msg) g_on_msg({ hdr.type, payload });
    }
    g_state = ConnState::Disconnected;
}

// ── Connect thread ────────────────────────────────────────────────────────────
static void connect_loop() {
    while (g_running) {
        if (g_state == ConnState::Connected) { Sleep(500); continue; }

        g_state = ConnState::Connecting;

        // Try to open the pipe
        HANDLE pipe = CreateFileA(
            LUNA_PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            g_state = ConnState::Disconnected;
            Sleep(1000); // retry every second
            continue;
        }

        // Set byte-mode
        DWORD mode = PIPE_READMODE_BYTE;
        SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

        g_pipe  = pipe;
        g_state = ConnState::Connected;

        // Notify connected
        if (g_on_msg) g_on_msg({ LunaMsgType::Status, "[LunaClient] Connected to DLL." });

        // Start reader — blocks until disconnect
        reader_loop();

        // Disconnected
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
        g_state = ConnState::Disconnected;

        if (g_on_msg) g_on_msg({ LunaMsgType::Status, "[LunaClient] Disconnected. Reconnecting..." });
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void connect(OnMessageCallback on_message) {
    if (g_running) return;
    g_on_msg  = std::move(on_message);
    g_running = true;
    g_thread  = std::thread(connect_loop);
}

void disconnect() {
    g_running = false;
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(g_pipe, nullptr);
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
    if (g_thread.joinable()) g_thread.join();
    g_state = ConnState::Disconnected;
}

bool send_script(const std::string& script) {
    if (g_state != ConnState::Connected) return false;
    return write_msg(g_pipe, LunaMsgType::Execute, script);
}

bool ping() {
    if (g_state != ConnState::Connected) return false;
    return write_msg(g_pipe, LunaMsgType::Ping, "");
}

ConnState state() { return g_state.load(); }
bool      connected() { return g_state == ConnState::Connected; }

} // namespace pipe_client
