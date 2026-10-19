#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <cstdint>
#include "MinHook.h"
#include "openvr.h"
#include "frame_policy.h"
#include "action_bridge.h"

namespace {
constexpr char ExpectedSha[] = "67615a629b1fd8b464b1d087166d91ba47b9427cd311ce665ca16eecc50df576";
constexpr uintptr_t SetupRva = 0x1d27220, ClassifierRva = 0x1052d00;
constexpr uintptr_t GripRva = 0x1d2a270, InputCallerReturnRva = 0x1d27378;
constexpr uintptr_t VrSystemGlobalRva = 0x627c300;
using SetupFn = void (*)(void*, void*);
using ClassifierFn = uint8_t (*)();
using GripFn = void (*)(void*, float);
SetupFn original_setup = nullptr;
ClassifierFn original_classifier = nullptr;
GripFn original_grip = nullptr;
uintptr_t image_base = 0;
HMODULE own_module = nullptr;
HANDLE log_file = INVALID_HANDLE_VALUE;
SRWLOCK log_lock = SRWLOCK_INIT, record_lock = SRWLOCK_INIT;
volatile LONG initialized = 0, status = 0;
bool patch_enabled = true, index_game_mode = true, full_actions = true;
SRWLOCK identity_lock = SRWLOCK_INIT;
ULONGLONG identity_checked = 0;
bool cached_frame_system = false;
unsigned log_lines = 0;

struct InputScope {
    bool frame = false;
    bool seen = false;
    bool index_selected = false;
    void* hand = nullptr;
};
thread_local InputScope input_scope;

struct HandRecord {
    void* hand = nullptr;
    bool frame = false;
    bool first = true;
    bool pressed = false;
    unsigned selector = 0;
    float last_value = 0;
    ULONGLONG last_log = 0;
    unsigned samples = 0;
};
HandRecord hands[16];

template<typename T> T field(void* object, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object)+offset);
}

void log_message(const char* format, ...) {
    if (log_file == INVALID_HANDLE_VALUE) return;
    char text[2048], line[2200];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    SYSTEMTIME time;
    GetSystemTime(&time);
    const int length = snprintf(line, sizeof(line),
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ %s\r\n",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,text);
    if (length <= 0) return;
    AcquireSRWLockExclusive(&log_lock);
    if (++log_lines <= 50000) {
        DWORD written;
        WriteFile(log_file,line,static_cast<DWORD>(strlen(line)),&written,nullptr);
        FlushFileBuffers(log_file);
    }
    ReleaseSRWLockExclusive(&log_lock);
}

bool initialize_log(wchar_t* ini_path) {
    wchar_t module_path[32768], directory[32768], output[32768];
    DWORD n = GetModuleFileNameW(own_module,module_path,32768);
    if (!n || n >= 32768) return false;
    wchar_t* slash = wcsrchr(module_path,L'\\');
    if (!slash) return false;
    *slash = 0;
    if (swprintf(ini_path,32768,L"%ls\\TWDFrameGrip.ini",module_path) < 0) return false;
    GetPrivateProfileStringW(L"FrameGrip",L"LogDirectory",module_path,directory,32768,ini_path);
    CreateDirectoryW(directory,nullptr);
    if (swprintf(output,32768,L"%ls\\TWDFrameGrip.log",directory) < 0) return false;
    log_file = CreateFileW(output,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                          nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    return log_file != INVALID_HANDLE_VALUE;
}

bool hash_executable(char* hex) {
    wchar_t path[32768];
    if (!GetModuleFileNameW(nullptr,path,32768)) return false;
    HANDLE file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                              nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32];
    auto buffer = static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(),0,1024*1024));
    bool ok = buffer && BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0) >= 0;
    if (ok) ok = BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0) >= 0;
    DWORD read = 0;
    while (ok) {
        if (!ReadFile(file,buffer,1024*1024,&read,nullptr)) {ok=false;break;}
        if (!read) break;
        ok = BCryptHashData(hash,buffer,read,0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash,digest,sizeof(digest),0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    if (buffer) HeapFree(GetProcessHeap(),0,buffer);
    CloseHandle(file);
    if (ok) {
        for (unsigned i=0;i<32;++i) snprintf(hex+i*2,3,"%02x",digest[i]);
        hex[64] = 0;
    }
    return ok;
}

struct Device {
    bool frame = false;
    bool connected = false;
    uint32_t index = vr::k_unTrackedDeviceIndexInvalid;
    char type[256] = {}, model[256] = {}, render[256] = {};
};

Device inspect_device(unsigned selector) {
    Device result;
    // This is the IVRSystem_020 pointer initialized by the game's SteamVR plugin.
    // Use the matching 1.5.17 ABI and never initialize a second VR client here.
    vr::IVRSystem* system = *reinterpret_cast<vr::IVRSystem**>(image_base+VrSystemGlobalRva);
    if (!system || selector > 1) return result;
    auto role = selector == 0 ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
    result.index = system->GetTrackedDeviceIndexForControllerRole(role);
    if (result.index == vr::k_unTrackedDeviceIndexInvalid) return result;
    const bool connected = system->IsTrackedDeviceConnected(result.index);
    result.connected = connected;
    const bool controller = system->GetTrackedDeviceClass(result.index) == vr::TrackedDeviceClass_Controller;
    const bool correct_role = system->GetControllerRoleForTrackedDeviceIndex(result.index) == role;
    vr::ETrackedPropertyError error;
    system->GetStringTrackedDeviceProperty(result.index,vr::Prop_ControllerType_String,result.type,sizeof(result.type),&error);
    if (error != vr::TrackedProp_Success) result.type[0] = 0;
    system->GetStringTrackedDeviceProperty(result.index,vr::Prop_ModelNumber_String,result.model,sizeof(result.model),&error);
    if (error != vr::TrackedProp_Success) result.model[0] = 0;
    system->GetStringTrackedDeviceProperty(result.index,vr::Prop_RenderModelName_String,result.render,sizeof(result.render),&error);
    if (error != vr::TrackedProp_Success) result.render[0] = 0;
    result.frame = correct_role && framegrip::is_frame_device(connected,controller,result.type,result.model,result.render);
    return result;
}

bool frame_system_active() {
    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&identity_lock);
    if (!identity_checked || now-identity_checked >= 250) {
        const Device left=inspect_device(0), right=inspect_device(1);
        const bool active=framegrip::use_index_game_mode(left.connected,left.frame,right.connected,right.frame);
        if (active != cached_frame_system) {
            log_message("GAME_CONTROLLER_MODE frame=%u selected=%s openvr_identity=unchanged left_type=\"%s\" right_type=\"%s\"",
                active,active?"Index":"original",left.type,right.type);
        }
        cached_frame_system=active;
        identity_checked=now;
    }
    const bool active=cached_frame_system;
    ReleaseSRWLockExclusive(&identity_lock);
    return active;
}

void register_hand(void* hand, unsigned selector, bool frame) {
    AcquireSRWLockExclusive(&record_lock);
    HandRecord* record = nullptr;
    for (auto& item : hands) if (item.hand == hand) {record=&item;break;}
    if (!record) for (auto& item : hands) if (!item.hand) {record=&item;break;}
    if (record) {
        *record = HandRecord{};
        record->hand=hand;record->selector=selector;record->frame=frame;
    }
    ReleaseSRWLockExclusive(&record_lock);
    if (!record) log_message("HAND_TRACKING_FULL hand=%p; telemetry disabled for this hand",hand);
}

uint8_t hooked_classifier() {
    // Capture the caller before invoking any helper. MinHook uses a jump, so
    // this remains the original game's return address.
    const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    const uint8_t original = original_classifier();
    const bool local = input_scope.hand && caller == image_base+InputCallerReturnRva;
    const bool game_index = patch_enabled && index_game_mode && frame_system_active();
    const uint8_t selected = game_index ? 4 : framegrip::select_input_type(original,patch_enabled,input_scope.frame,local);
    if (local) {
        input_scope.seen=true;
        input_scope.index_selected=selected==4;
        log_message("INPUT_SELECT hand=%p frame=%u patch_enabled=%u original_enum=%u selected_enum=%u changed=%u axis=%sGrip",
            input_scope.hand,input_scope.frame,patch_enabled,original,selected,selected!=original,
            selected==4?"Knuckles":"ordinary");
    }
    return selected;
}

void hooked_setup(void* hand, void* input_component) {
    const unsigned selector = field<uint8_t>(hand,0x7d4);
    const Device device = inspect_device(selector);
    log_message("HAND_SETUP hand=%p hand_selector=%u device=%u frame=%u type=\"%s\" model=\"%s\" render_model=\"%s\" input=%p",
        hand,selector,device.index,device.frame,device.type,device.model,device.render,input_component);
    register_hand(hand,selector,device.frame);
    const InputScope prior = input_scope;
    input_scope = InputScope{};
    input_scope.hand=hand;input_scope.frame=device.frame;
    original_setup(hand,input_component);
    log_message("HAND_SETUP_DONE hand=%p classifier_seen=%u index_axis=%u press=%.4f release=%.4f",
        hand,input_scope.seen,input_scope.index_selected,field<float>(hand,0xaf8),field<float>(hand,0xafc));
    input_scope=prior;
}

void hooked_grip(void* hand, float value) {
    const bool pressed_before = field<uint8_t>(hand,0xe28) != 0;
    original_grip(hand,value);
    const bool pressed_after = field<uint8_t>(hand,0xe28) != 0;
    const float consumed = field<float>(hand,0xe2c);
    const ULONGLONG now = GetTickCount64();
    bool emit=false;
    unsigned selector=0, samples=0;
    AcquireSRWLockExclusive(&record_lock);
    for (auto& item : hands) if (item.hand == hand && item.frame) {
        ++item.samples;
        const bool transition = pressed_before != pressed_after;
        if (item.first || transition || now-item.last_log>=2000 ||
            (now-item.last_log>=100 && std::fabs(value-item.last_value)>=0.025f)) {
            emit=true;item.first=false;item.last_log=now;item.last_value=value;
            item.pressed=pressed_after;selector=item.selector;samples=item.samples;
        }
        break;
    }
    ReleaseSRWLockExclusive(&record_lock);
    if (emit) log_message("GRIP hand=%p hand_selector=%u sample=%u incoming=%.4f consumed=%.4f pressed_before=%u pressed_after=%u press=%.4f release=%.4f",
        hand,selector,samples,value,consumed,pressed_before,pressed_after,
        field<float>(hand,0xaf8),field<float>(hand,0xafc));
    if(full_actions)actionbridge::poll(patch_enabled && frame_system_active());
}

bool verify_memory() {
    const unsigned char setup[] = {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x55,0x57,0x41,0x56,0x48,0x8b};
    const unsigned char classifier[] = {0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x56,0x48,0x8b};
    const unsigned char grip[] = {0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x01,0x48,0x8b,0xd9,0x0f,0x29,0x74,0x24};
    const unsigned char call[] = {0xe8,0x88,0xb9,0x32,0xff};
    const unsigned char branch[] = {0x75,0x73};
    return !memcmp(reinterpret_cast<void*>(image_base+SetupRva),setup,sizeof(setup)) &&
           !memcmp(reinterpret_cast<void*>(image_base+ClassifierRva),classifier,sizeof(classifier)) &&
           !memcmp(reinterpret_cast<void*>(image_base+GripRva),grip,sizeof(grip)) &&
           !memcmp(reinterpret_cast<void*>(image_base+0x1d27373),call,sizeof(call)) &&
           !memcmp(reinterpret_cast<void*>(image_base+0x1d273d4),branch,sizeof(branch));
}

bool create_hook(uintptr_t rva, void* hook, void** original) {
    const auto code=MH_CreateHook(reinterpret_cast<void*>(image_base+rva),hook,original);
    log_message("CREATE_HOOK rva=0x%llx status=%s",static_cast<unsigned long long>(rva),MH_StatusToString(code));
    return code==MH_OK;
}
}

void frame_log(const char* format, ...) {
    char text[1800];va_list args;va_start(args,format);
    vsnprintf(text,sizeof(text),format,args);va_end(args);log_message("%s",text);
}

extern "C" __declspec(dllexport) unsigned TWDFrameGripStatus() {
    return static_cast<unsigned>(InterlockedCompareExchange(&status,0,0));
}

extern "C" __declspec(dllexport) void InitializeASI() {
    if (InterlockedCompareExchange(&initialized,1,0)) return;
    wchar_t ini_path[32768], exe_path[32768];
    if (!initialize_log(ini_path)) {status=9;return;}
    patch_enabled=GetPrivateProfileIntW(L"FrameGrip",L"PatchEnabled",1,ini_path)!=0;
    index_game_mode=GetPrivateProfileIntW(L"FrameGrip",L"IndexGameMode",1,ini_path)!=0;
    full_actions=GetPrivateProfileIntW(L"FrameGrip",L"FullActions",1,ini_path)!=0;
    GetModuleFileNameW(nullptr,exe_path,32768);
    wchar_t* basename=wcsrchr(exe_path,L'\\');
    basename=basename?basename+1:exe_path;
    log_message("SESSION_START version=0.3.0 pid=%lu patch_enabled=%u index_game_mode=%u full_actions=%u openvr_identity=unchanged runtime_models=untouched",GetCurrentProcessId(),patch_enabled,index_game_mode,full_actions);
    if (_wcsicmp(basename,L"TWD-Win64-Shipping.exe")) {
        log_message("REFUSED unexpected process name");status=1;return;
    }
    char sha[65]={};
    if (!hash_executable(sha) || strcmp(sha,ExpectedSha)) {
        log_message("REFUSED executable SHA256=%s expected=%s",sha,ExpectedSha);status=2;return;
    }
    image_base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    log_message("BUILD_VERIFIED sha256=%s image_base=%p",sha,reinterpret_cast<void*>(image_base));
    if (!verify_memory()) {log_message("REFUSED runtime code bytes differ; no hooks installed");status=3;return;}
    auto code=MH_Initialize();
    if (code!=MH_OK) {log_message("REFUSED MinHook init=%s",MH_StatusToString(code));status=4;return;}
    bool ok=create_hook(SetupRva,reinterpret_cast<void*>(hooked_setup),reinterpret_cast<void**>(&original_setup)) &&
            create_hook(ClassifierRva,reinterpret_cast<void*>(hooked_classifier),reinterpret_cast<void**>(&original_classifier)) &&
            create_hook(GripRva,reinterpret_cast<void*>(hooked_grip),reinterpret_cast<void**>(&original_grip));
    if (!ok) {MH_Uninitialize();log_message("REFUSED hook creation failed; no hooks enabled");status=5;return;}
    if(full_actions && !actionbridge::install(image_base)) {
        MH_Uninitialize();log_message("REFUSED extra action hooks failed; no hooks enabled");status=5;return;
    }
    for (uintptr_t rva : {SetupRva,ClassifierRva,GripRva}) MH_QueueEnableHook(reinterpret_cast<void*>(image_base+rva));
    code=MH_ApplyQueued();
    if (code!=MH_OK) {
        // Do not unload: a partial enable failure can leave live detours. Keep
        // their trampolines resident and disable the behavioral override.
        patch_enabled=false;
        log_message("HOOK_ENABLE_FAILED status=%s behavior_override=disabled",MH_StatusToString(code));status=6;return;
    }
    status=10;
    log_message("HOOKS_READY scope=%s telemetry=game-grip-callback runtime_models=untouched",
        index_game_mode?"Frame-game-Index-mode":"Frame-hand-input-selection-only");
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        own_module=module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
