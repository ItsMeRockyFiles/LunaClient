#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../../offsets.h"
#include "../utils/memory.h"
#include "../lua/lua.h"
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Roblox engine wrapper — provides safe access to engine objects via offsets
// ─────────────────────────────────────────────────────────────────────────────

namespace roblox {

// ── Roblox string read helper ─────────────────────────────────────────────────
inline std::string read_string(uintptr_t addr) {
    return memory::read_rbx_string(addr);
}

// ── VisualEngine ─────────────────────────────────────────────────────────────
inline uintptr_t get_visual_engine() {
    uintptr_t ptr_addr = memory::rebase(offsets::VisualEngine::Pointer);
    return memory::read<uintptr_t>(ptr_addr);
}

inline uintptr_t get_fake_data_model() {
    uintptr_t ve = get_visual_engine();
    if (!ve) return 0;
    return memory::read<uintptr_t>(ve + offsets::VisualEngine::FakeDataModel);
}

// ── DataModel ────────────────────────────────────────────────────────────────
inline uintptr_t get_data_model() {
    uintptr_t fdm = get_fake_data_model();
    if (!fdm) return 0;
    return memory::read<uintptr_t>(fdm + offsets::FakeDataModel::RealDataModel);
}

inline uintptr_t get_workspace() {
    uintptr_t dm = get_data_model();
    if (!dm) return 0;
    return memory::read<uintptr_t>(dm + offsets::DataModel::Workspace);
}

inline int64_t get_place_id() {
    uintptr_t dm = get_data_model();
    if (!dm) return 0;
    return memory::read<int64_t>(dm + offsets::DataModel::PlaceId);
}

inline int64_t get_game_id() {
    uintptr_t dm = get_data_model();
    if (!dm) return 0;
    return memory::read<int64_t>(dm + offsets::DataModel::GameId);
}

inline std::string get_job_id() {
    uintptr_t dm = get_data_model();
    if (!dm) return {};
    return read_string(dm + offsets::DataModel::JobId);
}

// ── TaskScheduler ────────────────────────────────────────────────────────────
inline uintptr_t get_task_scheduler() {
    uintptr_t ptr_addr = memory::rebase(offsets::TaskScheduler::Pointer);
    return memory::read<uintptr_t>(ptr_addr);
}

inline double get_fps_cap() {
    uintptr_t ts = get_task_scheduler();
    if (!ts) return 0.0;
    return memory::read<double>(ts + offsets::TaskScheduler::MaxFps);
}

inline void set_fps_cap(double fps) {
    uintptr_t ts = get_task_scheduler();
    if (!ts) return;
    memory::write<double>(ts + offsets::TaskScheduler::MaxFps, fps);
}

// ── Instance helpers ─────────────────────────────────────────────────────────
inline std::string get_instance_name(uintptr_t instance) {
    if (!instance) return {};
    uintptr_t name_container = memory::read<uintptr_t>(instance + offsets::Instance::NameContainer);
    if (!name_container) return {};
    return read_string(name_container + offsets::Instance::Name);
}

inline std::string get_class_name(uintptr_t instance) {
    if (!instance) return {};
    uintptr_t desc = memory::read<uintptr_t>(instance + offsets::Instance::ClassDescriptor);
    if (!desc) return {};
    uintptr_t name_str = memory::read<uintptr_t>(desc + offsets::ClassDescriptor::ClassName);
    if (!name_str) return {};
    return memory::read_string(name_str);
}

inline uintptr_t get_parent(uintptr_t instance) {
    if (!instance) return 0;
    return memory::read<uintptr_t>(instance + offsets::Instance::Parent);
}

// Get children list [start, end)
inline std::vector<uintptr_t> get_children(uintptr_t instance) {
    if (!instance) return {};
    uintptr_t children_ptr = memory::read<uintptr_t>(instance + offsets::Instance::ChildrenStart);
    uintptr_t children_end = memory::read<uintptr_t>(instance + offsets::Instance::ChildrenEnd);
    if (!children_ptr || !children_end || children_end <= children_ptr) return {};

    size_t count = (children_end - children_ptr) / sizeof(uintptr_t);
    std::vector<uintptr_t> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        uintptr_t child = memory::read<uintptr_t>(children_ptr + i * sizeof(uintptr_t));
        if (child) result.push_back(child);
    }
    return result;
}

// Find first child with matching name (non-recursive)
inline uintptr_t find_child(uintptr_t instance, const std::string& name) {
    for (uintptr_t child : get_children(instance)) {
        if (get_instance_name(child) == name) return child;
    }
    return 0;
}

// Find a service by class name under DataModel
inline uintptr_t get_service(const std::string& class_name) {
    uintptr_t dm = get_data_model();
    if (!dm) return 0;
    for (uintptr_t child : get_children(dm)) {
        if (get_class_name(child) == class_name) return child;
    }
    return 0;
}

// ── Players service helpers ───────────────────────────────────────────────────
inline uintptr_t get_players_service() {
    return get_service("Players");
}

inline uintptr_t get_local_player() {
    uintptr_t players = get_players_service();
    if (!players) return 0;
    return memory::read<uintptr_t>(players + offsets::Players::LocalPlayer);
}

inline std::string get_player_name(uintptr_t player) {
    return get_instance_name(player);
}

inline int64_t get_player_user_id(uintptr_t player) {
    if (!player) return 0;
    return memory::read<int64_t>(player + offsets::Player::UserId);
}

// ── Humanoid helpers ─────────────────────────────────────────────────────────
inline float get_walk_speed(uintptr_t humanoid) {
    if (!humanoid) return 0.f;
    return memory::read<float>(humanoid + offsets::Humanoid::WalkSpeed);
}

inline void set_walk_speed(uintptr_t humanoid, float speed) {
    if (!humanoid) return;
    memory::write<float>(humanoid + offsets::Humanoid::WalkSpeed, speed);
}

inline float get_jump_power(uintptr_t humanoid) {
    if (!humanoid) return 0.f;
    return memory::read<float>(humanoid + offsets::Humanoid::JumpPower);
}

inline void set_jump_power(uintptr_t humanoid, float power) {
    if (!humanoid) return;
    memory::write<float>(humanoid + offsets::Humanoid::JumpPower, power);
}

inline float get_health(uintptr_t humanoid) {
    if (!humanoid) return 0.f;
    return memory::read<float>(humanoid + offsets::Humanoid::Health);
}

inline float get_max_health(uintptr_t humanoid) {
    if (!humanoid) return 0.f;
    return memory::read<float>(humanoid + offsets::Humanoid::MaxHealth);
}

// ── Camera helpers ────────────────────────────────────────────────────────────
inline float get_fov() {
    uintptr_t ws = get_workspace();
    if (!ws) return 0.f;
    uintptr_t camera = memory::read<uintptr_t>(ws + offsets::Workspace::CurrentCamera);
    if (!camera) return 0.f;
    return memory::read<float>(camera + offsets::Camera::FieldOfView);
}

inline void set_fov(float fov) {
    uintptr_t ws = get_workspace();
    if (!ws) return;
    uintptr_t camera = memory::read<uintptr_t>(ws + offsets::Workspace::CurrentCamera);
    if (!camera) return;
    memory::write<float>(camera + offsets::Camera::FieldOfView, fov);
}

// ── ScriptContext ─────────────────────────────────────────────────────────────
inline uintptr_t get_script_context() {
    return get_service("ScriptContext");
}

// ── Initialization ────────────────────────────────────────────────────────────
// Called once on DLL attach to resolve all lua API function pointers
bool initialize(lua_State* L);

// Get the current script execution lua_State (from a running script's thread)
lua_State* get_script_state();

} // namespace roblox
