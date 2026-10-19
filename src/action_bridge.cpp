#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "MinHook.h"
#include "openvr.h"
#include "action_bridge.h"

extern void frame_log(const char* format, ...);

namespace actionbridge {
namespace {
uintptr_t base=0;
using SetupFn=void(*)(void*,void*);
using ControllerSetupFn=void(*)(void*);
using BindActionFn=void*(*)(void*,uint64_t,unsigned char,void*,const void*);
using BindAxisFn=void*(*)(void*,uint64_t,void*,const void*);
SetupFn original_setup=nullptr;
ControllerSetupFn original_controller_setup=nullptr;
BindActionFn original_action=nullptr;
BindAxisFn original_axis=nullptr;
thread_local unsigned setup_depth=0;
thread_local bool polling=false;

struct WeakObject {
    void* object=nullptr;
    int32_t index=-1,serial=0;
};
// Verified against this build's FUObjectArray accesses: 65536 items/chunk,
// 24 bytes/item, index at UObject+0xc and serial at FUObjectItem+0x10.
unsigned char* object_item(int32_t index) {
    if(index<0 || index>=*reinterpret_cast<int32_t*>(base+0x607eb5c))return nullptr;
    auto chunks=*reinterpret_cast<unsigned char***>(base+0x607eb48);
    if(!chunks || !chunks[index>>16])return nullptr;
    return chunks[index>>16]+24*(index&65535);
}
WeakObject weak(void* object) {
    WeakObject result;
    if(!object)return result;
    const int32_t index=*reinterpret_cast<int32_t*>(static_cast<unsigned char*>(object)+0xc);
    auto item=object_item(index);
    if(item && *reinterpret_cast<void**>(item)==object) {
        result.object=object;result.index=index;
        result.serial=*reinterpret_cast<int32_t*>(item+0x10);
    }
    return result;
}
bool valid(const WeakObject& object) {
    auto item=object_item(object.index);
    return item && object.object && *reinterpret_cast<void**>(item)==object.object &&
        *reinterpret_cast<int32_t*>(item+0x10)==object.serial &&
        !(*reinterpret_cast<uint32_t*>(item+8)&0x30000000u);
}
struct Callback {
    WeakObject object,component;
    uintptr_t function=0;
    int32_t adjustment=0;
};
struct ExtraAction {
    const wchar_t* name;
    const char* path;
    bool analog;
    uint64_t fname=0;
    vr::VRActionHandle_t handle=0;
    bool pressed=false,handle_checked=false;
    Callback callback[2] = {};
};
ExtraAction actions[]={
#include <additional_actions.inc>
};
bool names_ready=false;
ULONGLONG last_poll=0;

void initialize_names() {
    if(names_ready)return;
    using MakeName=uint64_t*(*)(uint64_t*,const wchar_t*,unsigned);
    auto make_name=reinterpret_cast<MakeName>(base+0x1ff8bb0);
    for(auto& action:actions)make_name(&action.fname,action.name,1);
    names_ready=true;
}
void capture(void* component,uint64_t name,unsigned event,void* object,const void* method,bool analog) {
    if(!setup_depth || event>1 || !method)return;
    for(auto& action:actions)if(action.fname==name && action.analog==analog) {
        auto& callback=action.callback[event];
        callback.object=weak(object);callback.component=weak(component);
        std::memcpy(&callback.function,method,8);
        std::memcpy(&callback.adjustment,static_cast<const unsigned char*>(method)+8,4);
        frame_log("EXTRA_BIND action=\"%s\" event=%u analog=%u valid=%u",action.path,event,analog,
                  valid(callback.object)&&valid(callback.component));
        return;
    }
}
void enter_setup() {initialize_names();++setup_depth;}
void leave_setup() {--setup_depth;}
void setup(void* player,void* component) {
    enter_setup();original_setup(player,component);leave_setup();
}
void controller_setup(void* controller) {
    enter_setup();original_controller_setup(controller);leave_setup();
}
void* bind_action(void* component,uint64_t name,unsigned char event,void* object,const void* method) {
    void* result=original_action(component,name,event,object,method);
    capture(component,name,event,object,method,false);
    return result;
}
void* bind_axis(void* component,uint64_t name,void* object,const void* method) {
    void* result=original_axis(component,name,object,method);
    capture(component,name,0,object,method,true);
    return result;
}
bool invoke(const Callback& callback,bool analog,float value) {
    if(!callback.function || !valid(callback.object) || !valid(callback.component))return false;
    // The captured MSVC member pointer is the game's own thunk plus this adjustment.
    auto object=static_cast<unsigned char*>(callback.object.object)+callback.adjustment;
    if(analog)reinterpret_cast<void(*)(void*,float)>(callback.function)(object,value);
    else reinterpret_cast<void(*)(void*)>(callback.function)(object);
    return true;
}
}

bool install(uintptr_t image_base) {
    base=image_base;
    struct Hook {uintptr_t rva;void* detour;void** original;const unsigned char* bytes;size_t length;};
    const unsigned char setup_bytes[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57};
    const unsigned char controller_bytes[]={0x4c,0x8b,0xdc,0x55,0x41,0x54,0x49,0x8d};
    const unsigned char action_bytes[]={0x48,0x89,0x5c,0x24,0x08,0x55,0x48,0x8d};
    const unsigned char axis_bytes[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83};
    const Hook hooks[]={
        {0x1bfea60,reinterpret_cast<void*>(setup),reinterpret_cast<void**>(&original_setup),setup_bytes,sizeof(setup_bytes)},
        {0x1d16040,reinterpret_cast<void*>(controller_setup),reinterpret_cast<void**>(&original_controller_setup),controller_bytes,sizeof(controller_bytes)},
        {0xc80ce0,reinterpret_cast<void*>(bind_action),reinterpret_cast<void**>(&original_action),action_bytes,sizeof(action_bytes)},
        {0xc80c00,reinterpret_cast<void*>(bind_axis),reinterpret_cast<void**>(&original_axis),axis_bytes,sizeof(axis_bytes)},
    };
    for(const auto& hook:hooks) {
        if(std::memcmp(reinterpret_cast<void*>(base+hook.rva),hook.bytes,hook.length)) {
            frame_log("EXTRA_REFUSED runtime bytes differ rva=0x%llx",static_cast<unsigned long long>(hook.rva));return false;
        }
    }
    for(const auto& hook:hooks) {
        auto status=MH_CreateHook(reinterpret_cast<void*>(base+hook.rva),hook.detour,hook.original);
        frame_log("EXTRA_HOOK rva=0x%llx status=%s",static_cast<unsigned long long>(hook.rva),MH_StatusToString(status));
        if(status!=MH_OK || MH_QueueEnableHook(reinterpret_cast<void*>(base+hook.rva))!=MH_OK)return false;
    }
    return true;
}

void poll(bool frame_enabled) {
    if(!frame_enabled || !names_ready || polling)return;
    const auto now=GetTickCount64();
    if(now-last_poll<8)return;
    auto input=*reinterpret_cast<vr::IVRInput**>(base+0x627c368);
    if(!input)return;
    last_poll=now;polling=true;
    for(auto& action:actions) {
        const bool has_callback=valid(action.callback[0].object)||valid(action.callback[1].object);
        if(!has_callback)continue;
        if(!action.handle_checked) {
            auto error=input->GetActionHandle(action.path,&action.handle);
            action.handle_checked=error==vr::VRInputError_None;
            frame_log("EXTRA_HANDLE action=\"%s\" error=%d handle=%llu",action.path,error,static_cast<unsigned long long>(action.handle));
        }
        if(!action.handle)continue;
        if(action.analog) {
            vr::InputAnalogActionData_t data={};
            if(input->GetAnalogActionData(action.handle,&data,sizeof(data),0)==vr::VRInputError_None && data.bActive)
                invoke(action.callback[0],true,data.x);
        } else {
            vr::InputDigitalActionData_t data={};
            auto error=input->GetDigitalActionData(action.handle,&data,sizeof(data),0);
            if(error!=vr::VRInputError_None)continue;
            const bool pressed=data.bActive&&data.bState;
            if(pressed!=action.pressed) {
                const bool delivered=invoke(action.callback[pressed?0:1],false,0);
                frame_log("EXTRA_INPUT action=\"%s\" pressed=%u delivered=%u",action.path,pressed,delivered);
                action.pressed=pressed;
            }
        }
    }
    polling=false;
}
}
