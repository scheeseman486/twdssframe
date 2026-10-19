#pragma once
#include <cstdint>
namespace actionbridge {
bool install(std::uintptr_t image_base);
void poll(bool frame_enabled);
}
