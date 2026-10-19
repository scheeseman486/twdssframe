#pragma once
#include <cstring>
#include <cstdint>

namespace framegrip {
inline bool starts_with(const char* value, const char* prefix) {
    return value && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}
inline bool is_frame_device(bool connected, bool controller, const char* type,
                            const char* model, const char* render_model) {
    if (!connected || !controller || !type || !model || !render_model) return false;
    const bool frame_type = std::strcmp(type, "frame_controller") == 0;
    const bool legacy_type = std::strcmp(type, "roy") == 0;
    const bool frame_model = starts_with(model, "Roy_") || starts_with(model, "Steam Frame");
    const bool frame_render = starts_with(render_model, "{frame_controller}") ||
                              starts_with(render_model, "{roy}");
    return (frame_type || legacy_type) && frame_model && frame_render;
}
inline std::uint8_t select_input_type(std::uint8_t original, bool enabled,
                                     bool frame_hand, bool local_call) {
    return enabled && frame_hand && local_call ? 4 : original;
}
inline bool use_index_game_mode(bool left_connected, bool left_frame,
                                bool right_connected, bool right_frame) {
    return (left_frame || right_frame) &&
           (!left_connected || left_frame) && (!right_connected || right_frame);
}
}
