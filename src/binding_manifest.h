#pragma once
#include "picojson.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace framebindings {
using Json = picojson::value;
using Object = picojson::object;
using Array = picojson::array;
inline std::string lower(std::string value) {
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline bool parse(const std::string& text, Json& value, std::string& error) {
    error.clear();
    auto first = text.begin();
    if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) first += 3;
    auto end = picojson::parse(value, first, text.end(), &error);
    while (end != text.end() && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (error.empty() && end != text.end()) error = "Trailing data after JSON";
    return error.empty();
}
inline bool merge(const std::string& original, const std::string& definitions,
                  std::string& output, std::string& error) {
    Json manifest, additions;
    if (!parse(original, manifest, error) || !parse(definitions, additions, error)) return false;
    if (!manifest.is<Object>() || !manifest.get("actions").is<Array>() ||
        !manifest.get("default_bindings").is<Array>() || !additions.is<Array>()) {
        error = "Invalid action manifest structure"; return false;
    }
    auto& root = manifest.get<Object>();
    auto& defaults = root["default_bindings"].get<Array>();
    defaults.erase(std::remove_if(defaults.begin(), defaults.end(), [](const Json& row) {
        return row.is<Object>() && row.get("controller_type").is<std::string>() &&
               row.get("controller_type").get<std::string>() == "frame_controller";
    }), defaults.end());
    Object frame;
    frame["controller_type"] = Json(std::string("frame_controller"));
    frame["binding_url"] = Json(std::string("frame_controller_TWDFrameGrip.json"));
    defaults.insert(defaults.begin(), Json(frame));
    auto& actions = root["actions"].get<Array>();
    if (!root.count("localization")) root["localization"] = Json(Array{});
    if (!root["localization"].is<Array>()) { error = "Invalid localization"; return false; }
    auto& localization = root["localization"].get<Array>();
    Json* english = nullptr;
    for (auto& row : localization) {
        if (!row.is<Object>()) { error = "Invalid localization entry"; return false; }
        if (row.get("language_tag").is<std::string>() &&
            row.get("language_tag").get<std::string>() == "en_us") english = &row;
    }
    if (!english) {
        Object row; row["language_tag"] = Json(std::string("en_us"));
        localization.emplace_back(row); english = &localization.back();
    }
    for (const auto& addition : additions.get<Array>()) {
        if (!addition.is<Object>() || !addition.get("action").is<std::string>() || !addition.get("name").is<std::string>() ||
            !addition.get("type").is<std::string>() || !addition.get("context").is<std::string>()) {
            error = "Invalid embedded action"; return false;
        }
        const auto path = "/actions/main/in/" + addition.get("action").get<std::string>();
        Json* existing = nullptr;
        for (auto& action : actions) {
            if (!action.is<Object>() || !action.get("name").is<std::string>() || !action.get("type").is<std::string>()) {
                error = "Invalid action entry"; return false;
            }
            if (lower(action.get("name").get<std::string>()) == lower(path)) existing = &action;
        }
        if (existing && existing->get("type").get<std::string>() != addition.get("type").get<std::string>()) {
            error = "Conflicting action type: " + path; return false;
        }
        if (!existing) {
            Object action;
            action["name"] = Json(path); action["type"] = addition.get("type");
            action["requirement"] = Json(std::string("optional")); actions.emplace_back(action);
        }
        english->get<Object>()[path] = Json(addition.get("name").get<std::string>() +
                                           " [" + addition.get("context").get<std::string>() + "]");
    }
    output = manifest.serialize(true) + "\n";
    return true;
}
}
