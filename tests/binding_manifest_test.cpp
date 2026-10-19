#include "binding_manifest.h"
#include "embedded_bindings.h"
#include <cstdlib>
#include <iostream>

void require(bool ok) { if (!ok) { std::cerr << "Binding manifest check failed\n"; std::exit(1); } }
int main() {
    using namespace framebindings;
    const std::string source = R"({"actions":[{"name":"/actions/main/in/SprintToggle","type":"boolean"}],"default_bindings":[{"controller_type":"knuckles","binding_url":"custom.json"},{"controller_type":"frame_controller","binding_url":"old.json"}],"localization":[{"language_tag":"fr_fr","/actions/main":"Français"},{"language_tag":"en_us","/actions/main":"Custom name"}],"version":3})";
    std::string out, error, again;
    require(merge(source, embedded::actions, out, error));
    Json result; require(parse(out, result, error));
    require(result.get("version").get<double>() == 3);
    const auto& defaults = result.get("default_bindings").get<Array>();
    require(defaults.size() == 2 && defaults[1].get("binding_url").get<std::string>() == "custom.json");
    require(defaults[0].get("binding_url").get<std::string>() == "frame_controller_TWDFrameGrip.json");
    require(result.get("localization").get<Array>()[0].get("/actions/main").get<std::string>() == "Français");
    require(result.get("localization").get<Array>()[1].get("/actions/main").get<std::string>() == "Custom name");
    require(merge(out, embedded::actions, again, error) && out == again);
    require(!merge("{", embedded::actions, out, error));
    require(!merge(source+" garbage", embedded::actions, out, error));
    require(!merge(R"({"actions":[null],"default_bindings":[]})", embedded::actions, out, error));
    require(!merge(R"({"actions":[],"default_bindings":[],"localization":[null]})", embedded::actions, out, error));
    require(!merge(R"({"actions":[{"name":"/actions/main/in/SPRINTHOLD","type":"vector1"}],"default_bindings":[]})", embedded::actions, out, error));
    Json binding; require(parse(embedded::binding, binding, error));
    require(merge(source, embedded::actions, out, error)); require(parse(out, result, error));
    for (const auto& action : result.get("actions").get<Array>()) {
        if (lower(action.get("name").get<std::string>()) == "/actions/main/in/sprinthold") require(action.get("type").get<std::string>() == "boolean");
    }
    std::cout << "Manifest merge preserves unrelated defaults and localization, is idempotent, and rejects invalid or conflicting data.\n";
}
