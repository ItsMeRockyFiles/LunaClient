#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include "../../shared/protocol.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Pipe client — launcher side
//  Connects to the DLL's named pipe server, sends scripts, receives output
// ─────────────────────────────────────────────────────────────────────────────

namespace pipe_client {

enum class ConnState { Disconnected, Connecting, Connected };

struct Message {
    LunaMsgType type;
    std::string text;
};

// Callbacks
using OnMessageCallback = std::function<void(const Message&)>;

// Connect to the DLL pipe (non-blocking, starts background thread)
void connect(OnMessageCallback on_message);

// Disconnect and stop background thread
void disconnect();

// Send a script to the DLL for execution
bool send_script(const std::string& script);

// Ping the DLL
bool ping();

// Current connection state
ConnState state();

// Is currently connected
bool connected();

} // namespace pipe_client
