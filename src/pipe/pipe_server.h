#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include "../../shared/protocol.h"

// ─────────────────────────────────────────────────────────────────────────────
//  DLL-side named pipe server
//  Runs on a background thread.
//  Receives script execution requests from the launcher EXE.
//  Sends back print output and errors.
// ─────────────────────────────────────────────────────────────────────────────

namespace pipe_server {

// Callback types
using ExecuteCallback = std::function<void(const std::string& script)>;
using StatusCallback  = std::function<std::string()>; // returns status string

// Start the pipe server thread
void start(ExecuteCallback on_execute, StatusCallback on_status);

// Stop the pipe server
void stop();

// Send output/error back to the connected EXE
void send_output(const std::string& text);
void send_error(const std::string& text);

} // namespace pipe_server
