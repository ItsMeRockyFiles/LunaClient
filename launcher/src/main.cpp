// ─────────────────────────────────────────────────────────────────────────────
//  LunaClient Launcher — main.cpp
//  Win32 + DirectX 11 + ImGui application entry point
// ─────────────────────────────────────────────────────────────────────────────

// Dear ImGui: vendor/imgui/
// Place these files in vendor/imgui/:
//   imgui.h / imgui.cpp
//   imgui_draw.cpp / imgui_tables.cpp / imgui_widgets.cpp
//   backends/imgui_impl_win32.h / imgui_impl_win32.cpp
//   backends/imgui_impl_dx11.h  / imgui_impl_dx11.cpp
//
// DirectX 11 SDK comes with the Windows SDK.

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

#include "injector.h"
#include "pipe_client.h"
#include "../../shared/protocol.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Globals
// ─────────────────────────────────────────────────────────────────────────────
static ID3D11Device*           g_device           = nullptr;
static ID3D11DeviceContext*    g_context          = nullptr;
static IDXGISwapChain*         g_swap_chain       = nullptr;
static ID3D11RenderTargetView* g_main_rtv         = nullptr;
static HWND                    g_hwnd             = nullptr;
static bool                    g_running          = true;

// ── Console log ───────────────────────────────────────────────────────────────
struct LogEntry {
    LunaMsgType type;
    std::string text;
    std::string timestamp;
};
static std::deque<LogEntry> g_log;
static std::mutex           g_log_mtx;
static bool                 g_scroll_to_bottom = true;

static void push_log(LunaMsgType type, const std::string& text) {
    time_t t = time(nullptr);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    std::lock_guard<std::mutex> lock(g_log_mtx);
    // Split multi-line output
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty())
            g_log.push_back({ type, line, ts });
    }
    // Cap log at 500 lines
    while (g_log.size() > 500) g_log.pop_front();
    g_scroll_to_bottom = true;
}

// ── Script editor buffer ──────────────────────────────────────────────────────
static char  g_editor_buf[1024 * 512] = "-- LunaClient Script Editor\n-- Write your Lua script here and press Execute\n\nprint(\"Hello from LunaClient!\")\n";
static float g_editor_font_size = 14.f;

// ── Script tabs ───────────────────────────────────────────────────────────────
struct ScriptTab {
    char  name[64];
    char  buf[1024 * 512];
    bool  modified = false;
};
static std::vector<ScriptTab> g_tabs;
static int                    g_active_tab = 0;

// ── Injection state ───────────────────────────────────────────────────────────
static std::string g_inject_status_msg;
static bool        g_inject_success = false;

// ─────────────────────────────────────────────────────────────────────────────
//  D3D11 helpers
// ─────────────────────────────────────────────────────────────────────────────
static bool create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level;
    UINT flags = 0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &g_swap_chain, &g_device, &feature_level, &g_context)))
        return false;

    ID3D11Texture2D* back_buf = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    g_device->CreateRenderTargetView(back_buf, nullptr, &g_main_rtv);
    back_buf->Release();
    return true;
}

static void cleanup_device() {
    if (g_main_rtv) { g_main_rtv->Release(); g_main_rtv = nullptr; }
    if (g_swap_chain) { g_swap_chain->Release(); g_swap_chain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

static void resize_rtv() {
    if (g_main_rtv) { g_main_rtv->Release(); g_main_rtv = nullptr; }
    g_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* back_buf = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    g_device->CreateRenderTargetView(back_buf, nullptr, &g_main_rtv);
    back_buf->Release();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Luna color palette
// ─────────────────────────────────────────────────────────────────────────────
static ImVec4 col(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return { r / 255.f, g / 255.f, b / 255.f, a / 255.f };
}

namespace luna_colors {
    inline ImVec4 bg_dark     = col( 10,  10,  18);
    inline ImVec4 bg_mid      = col( 17,  17,  28);
    inline ImVec4 bg_panel    = col( 20,  20,  34);
    inline ImVec4 bg_hover    = col( 30,  30,  50);
    inline ImVec4 accent      = col(120,  80, 255);       // purple
    inline ImVec4 accent_dim  = col( 80,  50, 180);
    inline ImVec4 accent_glow = col(160, 120, 255, 180);
    inline ImVec4 ok          = col( 80, 220, 140);
    inline ImVec4 err         = col(255,  80,  80);
    inline ImVec4 warn        = col(255, 190,  50);
    inline ImVec4 text        = col(220, 220, 235);
    inline ImVec4 text_dim    = col(130, 130, 160);
    inline ImVec4 separator   = col( 40,  40,  65);
}

static void apply_luna_theme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 10.f;
    s.FrameRounding     = 6.f;
    s.PopupRounding     = 6.f;
    s.ScrollbarRounding = 6.f;
    s.GrabRounding      = 4.f;
    s.TabRounding       = 6.f;
    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 0.f;
    s.ItemSpacing       = { 8.f, 6.f };
    s.FramePadding      = { 10.f, 6.f };
    s.WindowPadding     = { 14.f, 14.f };
    s.ScrollbarSize     = 10.f;
    s.GrabMinSize       = 8.f;
    s.TabBarBorderSize  = 1.f;

    ImVec4* c = s.Colors;
    using namespace luna_colors;

    c[ImGuiCol_WindowBg]             = bg_dark;
    c[ImGuiCol_ChildBg]              = bg_mid;
    c[ImGuiCol_PopupBg]              = bg_panel;
    c[ImGuiCol_Border]               = separator;
    c[ImGuiCol_BorderShadow]         = { 0,0,0,0 };

    c[ImGuiCol_FrameBg]              = bg_panel;
    c[ImGuiCol_FrameBgHovered]       = bg_hover;
    c[ImGuiCol_FrameBgActive]        = bg_hover;

    c[ImGuiCol_TitleBg]              = bg_dark;
    c[ImGuiCol_TitleBgActive]        = bg_dark;
    c[ImGuiCol_TitleBgCollapsed]     = bg_dark;

    c[ImGuiCol_MenuBarBg]            = bg_mid;
    c[ImGuiCol_ScrollbarBg]          = bg_dark;
    c[ImGuiCol_ScrollbarGrab]        = accent_dim;
    c[ImGuiCol_ScrollbarGrabHovered] = accent;
    c[ImGuiCol_ScrollbarGrabActive]  = accent_glow;

    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent_glow;

    c[ImGuiCol_Button]               = accent_dim;
    c[ImGuiCol_ButtonHovered]        = accent;
    c[ImGuiCol_ButtonActive]         = accent_glow;

    c[ImGuiCol_Header]               = { accent_dim.x, accent_dim.y, accent_dim.z, 0.4f };
    c[ImGuiCol_HeaderHovered]        = { accent.x, accent.y, accent.z, 0.5f };
    c[ImGuiCol_HeaderActive]         = accent_dim;

    c[ImGuiCol_Separator]            = separator;
    c[ImGuiCol_SeparatorHovered]     = accent_dim;
    c[ImGuiCol_SeparatorActive]      = accent;

    c[ImGuiCol_ResizeGrip]           = accent_dim;
    c[ImGuiCol_ResizeGripHovered]    = accent;
    c[ImGuiCol_ResizeGripActive]     = accent_glow;

    c[ImGuiCol_Tab]                  = bg_mid;
    c[ImGuiCol_TabHovered]           = accent_dim;
    c[ImGuiCol_TabActive]            = accent;
    c[ImGuiCol_TabUnfocused]         = bg_dark;
    c[ImGuiCol_TabUnfocusedActive]   = bg_mid;

    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = text_dim;

    c[ImGuiCol_PlotLines]            = accent;
    c[ImGuiCol_PlotLinesHovered]     = accent_glow;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_PlotHistogramHovered] = accent_glow;
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI rendering
// ─────────────────────────────────────────────────────────────────────────────

static void render_titlebar() {
    using namespace luna_colors;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Gradient header bar
    dl->AddRectFilledMultiColor(
        { 0.f, 0.f }, { io.DisplaySize.x, 48.f },
        IM_COL32(15, 10, 35, 255),
        IM_COL32(20, 10, 50, 255),
        IM_COL32(10, 10, 30, 255),
        IM_COL32(10, 10, 25, 255));

    // Accent glow line
    dl->AddLine({ 0.f, 48.f }, { io.DisplaySize.x, 48.f },
        IM_COL32(120, 80, 255, 180), 1.5f);

    // Branding text
    ImGui::SetNextWindowPos({ 0.f, 0.f });
    ImGui::SetNextWindowSize({ io.DisplaySize.x, 48.f });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##titlebar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);

    ImGui::SetCursorPos({ 14.f, 10.f });
    ImGui::PushStyleColor(ImGuiCol_Text, accent_glow);
    ImGui::Text("LUNA");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 6.f);
    ImGui::PushStyleColor(ImGuiCol_Text, text);
    ImGui::Text("CLIENT");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 12.f);
    ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
    ImGui::Text("v1.0.0");
    ImGui::PopStyleColor();

    ImGui::End();
}

static void render_statusbar() {
    using namespace luna_colors;
    ImGuiIO& io = ImGui::GetIO();
    float bar_h = 26.f;
    float bar_y = io.DisplaySize.y - bar_h;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled({ 0.f, bar_y }, io.DisplaySize,
        IM_COL32(10, 10, 20, 230));
    dl->AddLine({ 0.f, bar_y }, { io.DisplaySize.x, bar_y },
        IM_COL32(40, 40, 65, 255), 1.f);

    ImGui::SetNextWindowPos({ 0.f, bar_y });
    ImGui::SetNextWindowSize({ io.DisplaySize.x, bar_h });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);

    // Connection dot
    bool conn = pipe_client::connected();
    ImGui::SetCursorPos({ 10.f, 5.f });
    ImGui::PushStyleColor(ImGuiCol_Text, conn ? ok : err);
    ImGui::Text(conn ? "●  Connected" : "●  Not Attached");
    ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 20.f);
    ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
    ImGui::Text("END key to eject DLL");
    ImGui::PopStyleColor();

    ImGui::End();
}

static void render_left_panel(float panel_w, float top_y, float panel_h) {
    using namespace luna_colors;
    ImGui::SetNextWindowPos({ 0.f, top_y });
    ImGui::SetNextWindowSize({ panel_w, panel_h });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##left_panel", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

    // ── Attach section ─────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, accent_glow);
    ImGui::Text("ATTACH");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    DWORD rbx_pid = injector::find_roblox_pid();
    bool roblox_found = (rbx_pid != 0);

    ImGui::PushStyleColor(ImGuiCol_Text, roblox_found ? ok : err);
    ImGui::Text(roblox_found ? "● Roblox found (PID %lu)" : "● Roblox not found", rbx_pid);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    bool conn = pipe_client::connected();
    float btn_w = panel_w - 28.f;

    if (conn) {
        ImGui::PushStyleColor(ImGuiCol_Button, col(60, 40, 120));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col(80, 55, 160));
        ImGui::Button("  Attached  ##attach_btn", { btn_w, 36.f });
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button("  Inject DLL  ##inject_btn", { btn_w, 36.f })) {
            // Get dll path next to launcher exe
            char exe_path[MAX_PATH];
            GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
            std::filesystem::path dll_path =
                std::filesystem::path(exe_path).parent_path() / "LunaClient.dll";

            auto status = injector::inject_into_roblox(dll_path.string());
            g_inject_success   = (status.result == injector::InjectResult::Success ||
                                   status.result == injector::InjectResult::AlreadyInjected);
            g_inject_status_msg = status.message;
            push_log(g_inject_success ? LunaMsgType::Status : LunaMsgType::Error,
                     "[Injector] " + status.message);
        }
    }

    if (!g_inject_status_msg.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, g_inject_success ? ok : err);
        ImGui::TextWrapped("%s", g_inject_status_msg.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ── Script actions ─────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, accent_glow);
    ImGui::Text("SCRIPTS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(" + New Tab", { btn_w, 30.f })) {
        ScriptTab tab;
        snprintf(tab.name, sizeof(tab.name), "Script %d", (int)g_tabs.size() + 1);
        snprintf(tab.buf, sizeof(tab.buf), "-- New Script\nprint(\"hello\")\n");
        g_tabs.push_back(tab);
        g_active_tab = (int)g_tabs.size() - 1;
    }

    ImGui::Spacing();

    // Script tab list
    for (int i = 0; i < (int)g_tabs.size(); ++i) {
        bool active = (i == g_active_tab);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, accent_dim);

        if (ImGui::Button(g_tabs[i].name, { btn_w, 28.f })) g_active_tab = i;

        if (active) ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ── Settings ───────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, accent_glow);
    ImGui::Text("SETTINGS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Font size");
    ImGui::SliderFloat("##fontsize", &g_editor_font_size, 10.f, 24.f, "%.0f px");

    ImGui::End();
}

static void render_editor(float left_w, float top_y, float w, float h) {
    using namespace luna_colors;
    ImGui::SetNextWindowPos({ left_w, top_y });
    ImGui::SetNextWindowSize({ w, h });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##editor_panel", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings);

    // ── Tab bar ────────────────────────────────────────────────────────────────
    if (g_tabs.empty()) {
        // Default tab
        ScriptTab tab;
        strncpy_s(tab.name, "Script 1", sizeof(tab.name));
        strncpy_s(tab.buf, g_editor_buf, sizeof(tab.buf));
        g_tabs.push_back(tab);
        g_active_tab = 0;
    }

    if (ImGui::BeginTabBar("##script_tabs")) {
        for (int i = 0; i < (int)g_tabs.size(); ++i) {
            bool open = true;
            std::string tab_label = std::string(g_tabs[i].name)
                + (g_tabs[i].modified ? " *" : "")
                + "##tab" + std::to_string(i);

            if (ImGui::BeginTabItem(tab_label.c_str(), g_tabs.size() > 1 ? &open : nullptr)) {
                g_active_tab = i;
                ImGui::EndTabItem();
            }
            if (!open && g_tabs.size() > 1) {
                g_tabs.erase(g_tabs.begin() + i);
                if (g_active_tab >= (int)g_tabs.size()) g_active_tab = (int)g_tabs.size() - 1;
                --i;
            }
        }
        ImGui::EndTabBar();
    }

    // ── Toolbar buttons ────────────────────────────────────────────────────────
    float btn_h = 32.f;
    bool conn = pipe_client::connected();

    // Execute
    ImGui::PushStyleColor(ImGuiCol_Button,        col(60, 170,  80));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col(80, 200, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   col(50, 140,  60));
    if (!conn) ImGui::BeginDisabled();
    if (ImGui::Button("  ▶  Execute", { 120.f, btn_h })) {
        if (!g_tabs.empty()) {
            std::string script = g_tabs[g_active_tab].buf;
            pipe_client::send_script(script);
            push_log(LunaMsgType::Status, "[Execute] Script sent.");
        }
    }
    if (!conn) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 8.f);

    // Clear editor
    ImGui::PushStyleColor(ImGuiCol_Button,        col(60,  50,  90));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col(80,  65, 120));
    if (ImGui::Button("  Clear  ##clear_editor", { 80.f, btn_h })) {
        if (!g_tabs.empty()) memset(g_tabs[g_active_tab].buf, 0, sizeof(ScriptTab::buf));
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.f, 8.f);

    // Save script
    ImGui::PushStyleColor(ImGuiCol_Button,        col(50,  80, 130));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col(65, 105, 170));
    if (ImGui::Button("  Save  ##save_script", { 80.f, btn_h })) {
        if (!g_tabs.empty()) {
            char exe_path[MAX_PATH];
            GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
            std::filesystem::path save_dir =
                std::filesystem::path(exe_path).parent_path() / "scripts";
            std::filesystem::create_directories(save_dir);

            std::string fname = std::string(g_tabs[g_active_tab].name) + ".lua";
            std::ofstream f(save_dir / fname, std::ios::trunc);
            f << g_tabs[g_active_tab].buf;
            push_log(LunaMsgType::Status, "[Save] Saved to scripts/" + fname);
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.f, 8.f);

    // Load from file
    ImGui::PushStyleColor(ImGuiCol_Button,        col(50,  80, 130));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col(65, 105, 170));
    if (ImGui::Button("  Load  ##load_script", { 80.f, btn_h })) {
        // Simple: open a common dialog
        char file_name[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = g_hwnd;
        ofn.lpstrFile   = file_name;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrFilter = "Lua Scripts\0*.lua\0All Files\0*.*\0";
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            std::ifstream f(file_name, std::ios::binary);
            if (f.is_open()) {
                std::ostringstream ss; ss << f.rdbuf();
                std::string src = ss.str();
                if (!g_tabs.empty()) {
                    strncpy_s(g_tabs[g_active_tab].buf, src.c_str(), sizeof(ScriptTab::buf) - 1);
                }
            }
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Code editor ────────────────────────────────────────────────────────────
    float editor_h = h - btn_h - 20.f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, col(12, 12, 22));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);

    if (!g_tabs.empty()) {
        ImVec2 editor_size = { w - 28.f, editor_h - 60.f };
        if (ImGui::InputTextMultiline("##code_editor",
            g_tabs[g_active_tab].buf, sizeof(ScriptTab::buf),
            editor_size,
            ImGuiInputTextFlags_AllowTabInput)) {
            g_tabs[g_active_tab].modified = true;
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
}

static void render_console(float left_w, float top_y, float w, float h) {
    using namespace luna_colors;
    ImGui::SetNextWindowPos({ left_w, top_y });
    ImGui::SetNextWindowSize({ w, h });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##console", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings);

    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, accent_glow);
    ImGui::Text("CONSOLE");
    ImGui::PopStyleColor();
    ImGui::SameLine(w - 80.f);
    if (ImGui::SmallButton("Clear##console_clear")) {
        std::lock_guard<std::mutex> lock(g_log_mtx);
        g_log.clear();
    }
    ImGui::Separator();

    // Log area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, col(10, 10, 18));
    ImGui::BeginChild("##log_area", { w - 28.f, h - 56.f }, false, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(g_log_mtx);
        for (auto& entry : g_log) {
            ImVec4 c = text;
            const char* prefix = "";
            switch (entry.type) {
                case LunaMsgType::Output: c = text;   prefix = "  "; break;
                case LunaMsgType::Error:  c = err;    prefix = "✗ "; break;
                case LunaMsgType::Status: c = text_dim; prefix = "• "; break;
                default: break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::Text("[%s]", entry.timestamp.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0.f, 6.f);
            ImGui::PushStyleColor(ImGuiCol_Text, c);
            ImGui::TextUnformatted((std::string(prefix) + entry.text).c_str());
            ImGui::PopStyleColor();
        }
    }

    if (g_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.f);
        g_scroll_to_bottom = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main render frame
// ─────────────────────────────────────────────────────────────────────────────
static void render_frame() {
    ImGuiIO& io = ImGui::GetIO();
    float W = io.DisplaySize.x;
    float H = io.DisplaySize.y;

    float title_h  = 48.f;
    float status_h = 26.f;
    float left_w   = 200.f;
    float console_h= 200.f;
    float content_h = H - title_h - status_h;
    float editor_h  = content_h - console_h;

    render_titlebar();
    render_statusbar();
    render_left_panel(left_w, title_h, content_h);
    render_editor(left_w, title_h, W - left_w, editor_h);
    render_console(left_w, title_h + editor_h, W - left_w, console_h);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Win32 message pump
// ─────────────────────────────────────────────────────────────────────────────
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_device && wp != SIZE_MINIMIZED) resize_rtv();
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────────────────────
INT APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, INT) {
    // ── Create window ──────────────────────────────────────────────────────────
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"LunaClientLauncher";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        0, L"LunaClientLauncher", L"LunaClient",
        WS_OVERLAPPEDWINDOW, 100, 100, 1100, 720,
        nullptr, nullptr, hInstance, nullptr);

    if (!create_device(g_hwnd)) {
        DestroyWindow(g_hwnd);
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    // ── ImGui setup ───────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // no imgui.ini

    // Load font — use embedded default, or load JetBrains Mono if available
    io.Fonts->AddFontDefault();

    apply_luna_theme();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    // ── Connect pipe client ───────────────────────────────────────────────────
    pipe_client::connect([](const pipe_client::Message& msg) {
        push_log(msg.type, msg.text);
    });

    // ── Message loop ──────────────────────────────────────────────────────────
    ImVec4 clear_color = { 0.04f, 0.04f, 0.08f, 1.f };

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        render_frame();

        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_main_rtv, nullptr);
        g_context->ClearRenderTargetView(g_main_rtv,
            reinterpret_cast<const float*>(&clear_color));
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0); // vsync
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    pipe_client::disconnect();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}
