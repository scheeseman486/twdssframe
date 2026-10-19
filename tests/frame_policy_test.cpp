#include "frame_policy.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace framegrip;
    assert(is_frame_device(true,true,"frame_controller","Roy_Left","{frame_controller}frame_controller_left"));
    assert(is_frame_device(true,true,"frame_controller","Roy_Right","{frame_controller}frame_controller_right"));
    assert(!is_frame_device(false,true,"frame_controller","Roy_Left","{frame_controller}frame_controller_left"));
    assert(!is_frame_device(true,false,"frame_controller","Roy_Left","{frame_controller}frame_controller_left"));
    assert(!is_frame_device(true,true,"knuckles","Knuckles Left","{indexcontroller}valve_controller_knu_1_0_left"));
    assert(!is_frame_device(true,true,"vive_controller","VIVE Controller Pro MV","vr_controller_vive_1_5"));
    assert(!is_frame_device(true,true,"frame_controller","Unknown","{frame_controller}frame_controller_left"));
    for (uint8_t original=0;original<16;++original) {
        assert(select_input_type(original,true,true,true)==4);
        assert(select_input_type(original,false,true,true)==original);
        assert(select_input_type(original,true,false,true)==original);
        assert(select_input_type(original,true,true,false)==original);
    }
    assert(use_index_game_mode(true,true,true,true));
    assert(use_index_game_mode(true,true,false,false));
    assert(use_index_game_mode(false,false,true,true));
    assert(!use_index_game_mode(false,false,false,false));
    assert(!use_index_game_mode(true,true,true,false));
    assert(!use_index_game_mode(true,false,true,true));
    assert(!use_index_game_mode(true,false,true,false));
    assert(use_registered_frame_mode(true,false,false)); // Known Frame, no assigned hands yet.
    assert(use_registered_frame_mode(false,true,false)); // Temporary disconnect after verification.
    assert(!use_registered_frame_mode(false,false,false)); // No Frame evidence.
    assert(!use_registered_frame_mode(true,false,true)); // Mixed connected controllers.
    assert(!use_registered_frame_mode(false,true,true)); // A new non-Frame controller still wins.
    puts("Frame identity gate, local selection, and game-wide Index mode gate passed.");
}
