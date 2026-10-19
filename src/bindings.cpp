#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <exception>
#include "bindings.h"
#include "binding_manifest.h"
#include "embedded_bindings.h"

extern void frame_log(const char*, ...);

namespace framebindings {
namespace {
bool exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
bool read(const std::wstring& path, std::string& data) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 4*1024*1024;
    if (ok) {
        data.resize(static_cast<size_t>(size.QuadPart)); DWORD count = 0;
        ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &count, nullptr) && count == data.size();
    }
    CloseHandle(file); return ok;
}
bool write(const std::wstring& path, const std::string& data) {
    const auto temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD count = 0;
    bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &count, nullptr) &&
              count == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temporary.c_str());
    return ok;
}
std::string hash(const std::string& data) {
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE handle = nullptr;
    unsigned char digest[32]; char text[65]{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(algorithm, &handle, nullptr, 0, nullptr, 0, 0) >= 0;
    if (ok) ok = BCryptHashData(handle, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                              static_cast<ULONG>(data.size()), 0) >= 0;
    if (ok) ok = BCryptFinishHash(handle, digest, sizeof(digest), 0) >= 0;
    if (handle) BCryptDestroyHash(handle);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (ok) for (unsigned i=0; i<32; ++i) snprintf(text+i*2, 3, "%02x", digest[i]);
    return ok ? text : "";
}
std::string field(const Json& state, const char* key) {
    return state.get(key).is<std::string>() ? state.get(key).get<std::string>() : "";
}
bool fail(const char* reason) { frame_log("BINDINGS_REFUSED %s winerror=%lu", reason, GetLastError()); return false; }

bool manage(const wchar_t* executable, bool restore) {
    std::wstring directory(executable);
    for (unsigned i=0; i<3; ++i) {
        auto slash = directory.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return fail("invalid executable path");
        directory.resize(slash);
    }
    directory += L"\\Config\\SteamVRBindings\\";
    const auto manifest = directory + L"steamvr_manifest.json";
    const auto binding = directory + L"frame_controller_TWDFrameGrip.json";
    const auto backup_manifest = directory + L"TWDFrameGrip.original-manifest.json";
    const auto backup_binding = directory + L"TWDFrameGrip.original-binding.json";
    const auto state_path = directory + L"TWDFrameGrip.binding-state.json";
    std::string source, current_binding, state_text, error;
    if (!read(manifest, source)) return fail("cannot read game action manifest");
    const bool binding_exists = exists(binding);
    if (binding_exists && !read(binding, current_binding)) return fail("cannot read existing Frame default");
    const auto source_hash = hash(source);
    const auto binding_hash = binding_exists ? hash(current_binding) : std::string{};
    if (source_hash.empty() || (binding_exists && binding_hash.empty())) return fail("hash failure");
    Json state(Object{});
    const bool recorded = exists(state_path);
    if (recorded) {
        if (!read(state_path, state_text) || !parse(state_text, state, error) || !state.is<Object>() ||
            !state.get("binding_existed").is<bool>() || !state.get("pending").is<bool>())
            return fail("invalid binding recovery record");
        const bool pending = state.get("pending").get<bool>();
        if (source_hash != field(state, "manifest_sha256") &&
            !(pending && source_hash == field(state, "previous_manifest_sha256")))
            return fail("action manifest changed since installation; preserving it");
        if (binding_hash != field(state, "binding_sha256") &&
            !(pending && binding_hash == field(state, "previous_binding_sha256")))
            return fail("Frame default changed since installation; preserving it");
        std::string original;
        if (!read(backup_manifest, original) || hash(original) != field(state, "original_manifest_sha256"))
            return fail("original manifest backup missing or changed");
        if (state.get("binding_existed").get<bool>() &&
            (!read(backup_binding, original) || hash(original) != field(state, "original_binding_sha256")))
            return fail("original binding backup missing or changed");
    } else if (restore) {
        frame_log("BINDINGS_RESTORE no automatic installation to restore"); return true;
    } else {
        if (exists(backup_manifest) || exists(backup_binding)) return fail("unrecorded binding backup exists");
        state.get<Object>()["binding_existed"] = Json(binding_exists);
        state.get<Object>()["original_manifest_sha256"] = Json(source_hash);
        state.get<Object>()["original_binding_sha256"] = Json(binding_hash);
    }
    std::string merged;
    if (!restore && !merge(source, embedded::actions, merged, error)) {
        frame_log("BINDINGS_REFUSED invalid manifest: %s", error.c_str()); return false;
    }
    if (restore) {
        // A recovery record remains valid if a previous restore stopped between writes.
        std::string original;
        if (!read(backup_manifest, original)) return fail("cannot read original manifest");
        state.get<Object>()["previous_manifest_sha256"] = Json(source_hash);
        state.get<Object>()["previous_binding_sha256"] = Json(binding_hash);
        state.get<Object>()["manifest_sha256"] = Json(field(state, "original_manifest_sha256"));
        state.get<Object>()["binding_sha256"] = Json(field(state, "original_binding_sha256"));
        state.get<Object>()["pending"] = Json(true);
        if (!write(state_path, state.serialize(true))) return fail("cannot record restore");
        if (!write(manifest, original)) return fail("cannot restore manifest");
        if (state.get("binding_existed").get<bool>()) {
            if (!read(backup_binding, original) || !write(binding, original)) return fail("cannot restore binding");
        } else if (binding_exists && !DeleteFileW(binding.c_str())) return fail("cannot remove generated binding");
        // Retire the active record after both originals have been restored.
        if (!MoveFileExW(state_path.c_str(), (state_path+L".restored").c_str(), MOVEFILE_REPLACE_EXISTING))
            return fail("cannot retire binding recovery record");
        DeleteFileW(backup_manifest.c_str()); DeleteFileW(backup_binding.c_str());
        frame_log("BINDINGS_RESTORED original manifest and Frame default restored"); return true;
    }
    const std::string supplied = std::string(embedded::binding) + "\n";
    const auto merged_hash = hash(merged), supplied_hash = hash(supplied);
    if (merged_hash.empty() || supplied_hash.empty()) return fail("hash failure");
    if (recorded && source_hash == merged_hash && binding_hash == supplied_hash &&
        !state.get("pending").get<bool>()) {
        frame_log("BINDINGS_READY embedded Frame default and additional actions already installed"); return true;
    }
    if (!recorded && (!write(backup_manifest, source) ||
        (binding_exists && !write(backup_binding, current_binding)))) return fail("cannot back up original bindings");
    auto& record = state.get<Object>();
    record["previous_manifest_sha256"] = Json(source_hash);
    record["previous_binding_sha256"] = Json(binding_hash);
    record["manifest_sha256"] = Json(merged_hash); record["binding_sha256"] = Json(supplied_hash);
    record["pending"] = Json(true);
    if (!write(state_path, state.serialize(true))) return fail("cannot record binding update");
    if (!write(binding, supplied) || !write(manifest, merged)) return fail("cannot install embedded bindings");
    record["pending"] = Json(false);
    if (!write(state_path, state.serialize(true))) return fail("cannot finalize binding update");
    frame_log("BINDINGS_READY installed embedded Frame default and additional actions; originals backed up");
    return true;
}
}
bool prepare(const wchar_t* executable, bool restore) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\TWDFrameGripBindingInstall");
    if (!mutex) return fail("cannot create binding lock");
    const DWORD wait = WaitForSingleObject(mutex, 10000);
    bool ok = false;
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
        try { ok = manage(executable, restore); }
        catch (const std::exception& exception) { frame_log("BINDINGS_REFUSED %s", exception.what()); }
        ReleaseMutex(mutex);
    } else fail("binding lock timed out");
    CloseHandle(mutex); return ok;
}
}
