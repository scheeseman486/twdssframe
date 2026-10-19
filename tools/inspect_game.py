#!/usr/bin/env python3
"""Read the installed game and emit a small JSON investigation report.

Uses the Python standard library. Does not modify the game or SteamVR.
String offsets are file offsets, not runtime addresses or verified hook sites.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


MARKERS = (
    "GetVRControllerType",
    "GetVRControllerTypeTemp_RealOculusTypes",
    "ESDIVRControllerType::VRController_Knuckles",
    "GetGripInputValue",
    "GetGripInputMode",
    "KnucklesGripPressThreshold",
    "KnucklesGripReleaseThreshold",
    "GripPressThreshold",
    "GripReleaseThreshold",
    "KnucklesLeftGrip",
    "KnucklesRightGrip",
    "knuckles",
    "IVRSystem_020",
    "IVRInput_007",
)


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def grip_binding(path):
    binding = load_json(path)
    sets = {}
    for name, actions in binding.get("bindings", {}).items():
        sets[name] = {
            "grip_sources": [
                item for item in actions.get("sources", [])
                if any(term in json.dumps(item).lower()
                       for term in ("grip", "finger", "skeleton"))
            ],
            "skeleton": actions.get("skeleton", []),
        }
    return {
        "controller_type": binding.get("controller_type"),
        "options": binding.get("options"),
        "action_sets": sets,
    }


def inspect(game_root, steamvr_root):
    executable = game_root / "TWD/Binaries/Win64/TWD-Win64-Shipping.exe"
    bindings = game_root / "TWD/Config/SteamVRBindings"
    binary = executable.read_bytes()
    if binary[:2] != b"MZ":
        raise ValueError(f"Expected a Windows executable: {executable}")
    manifest = load_json(bindings / "steamvr_manifest.json")
    markers = []
    for marker in MARKERS:
        for encoding in ("ascii", "utf-16-le"):
            # Require both boundaries to distinguish GripPressThreshold from
            # the suffix of KnucklesGripPressThreshold.
            needle = ("\0" + marker + "\0").encode(encoding)
            offset = binary.find(needle)
            if offset >= 0:
                markers.append({"name": marker, "encoding": encoding,
                                "file_offset": hex(offset + len("\0".encode(encoding)))})
    report = {
        "game_root": str(game_root),
        "executable": {
            "path": str(executable),
            "size_bytes": len(binary),
            "sha256": hashlib.sha256(binary).hexdigest(),
        },
        "markers": markers,
        "grip_actions": [
            action for action in manifest.get("actions", [])
            if any(term in action.get("name", "").lower()
                   for term in ("grip", "skeleton"))
        ],
        "default_bindings": manifest.get("default_bindings", []),
        "bindings": {
            name: grip_binding(bindings / name)
            for name in ("knuckles.json", "oculus_touch_TWD.json")
            if (bindings / name).is_file()
        },
        "available_tools": {
            name: shutil.which(name) for name in (
                "cmake", "ninja", "clang", "clang-cl", "lld-link",
                "x86_64-w64-mingw32-g++", "objdump", "wine",
            )
        },
    }
    appmanifest = game_root.parent.parent / "appmanifest_916840.acf"
    if appmanifest.is_file():
        match = re.search(r'"buildid"\s+"([0-9]+)"',
                          appmanifest.read_text(encoding="utf-8"))
        if match:
            report["steam_build_id"] = match.group(1)
    if steamvr_root:
        # Current hardware uses frame_controller. Some installations also retain
        # the older roy profile, whose capabilities differ from the active one.
        driver = "frame_controller"
        profile_root = steamvr_root / f"drivers/{driver}/resources/input"
        profile_path = profile_root / f"{driver}_profile.json"
        if not profile_path.is_file():
            driver = "roy_controller"
            profile_root = steamvr_root / "drivers/roy/resources/input"
            profile_path = profile_root / "roy_controller_profile.json"
        if profile_path.is_file():
            profile = load_json(profile_path)
            report["frame_profile"] = {
                "path": str(profile_path),
                "controller_type": profile.get("controller_type"),
                "grip_inputs": {
                    name: value
                    for name, value in profile.get("input_source", {}).items()
                    if "grip" in name
                },
            }
        remap_path = profile_root / f"{driver}_remapping.json"
        if remap_path.is_file():
            remap = load_json(remap_path)
            report["frame_index_remapping"] = [
                layout for layout in remap.get("layouts", [])
                if layout.get("from_controller_type") == "knuckles"
            ]
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path,
                        default=Path(__file__).resolve().parents[1] / "paths.local.json")
    parser.add_argument("--game-root", type=Path)
    parser.add_argument("--steamvr-root", type=Path)
    args = parser.parse_args()
    config = load_json(args.config) if args.config.is_file() else {}
    game_root = args.game_root or config.get("game_root")
    steamvr_root = args.steamvr_root or config.get("steamvr_root")
    if not game_root:
        parser.error("Provide --game-root or game_root in the config file")
    try:
        report = inspect(Path(game_root), Path(steamvr_root) if steamvr_root else None)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Inspection failed: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
