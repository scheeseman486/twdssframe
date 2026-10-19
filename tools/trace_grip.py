#!/usr/bin/env python3
"""Reproduce the static grip trace for the verified installed game build.

Writes disassembly and a JSON evidence report under artifacts/native-trace.
Never patches the executable or a running process. The candidate replacement
is decoded only in a separate bytes object for review.
"""
import hashlib
import json
from pathlib import Path
import struct

from native_image import Image


EXPECTED_SHA = "67615a629b1fd8b464b1d087166d91ba47b9427cd311ce665ca16eecc50df576"
FUNCTIONS = {
    "classifier_exec_wrapper": (0x10D2BA0, None),
    "controller_classifier": (0x1052D00, None),
    "steamvr_system_name": (0x10EC880, 14),
    "steamvr_version_string": (0x10F3910, None),
    "steamvr_property_string": (0x10F02A0, None),
    "hand_constructor": (0x1D25DC0, None),
    "hand_input_setup": (0x1D27220, None),
    "grip_delegate": (0xC89330, 9),
    "game_hand_input_grip": (0x1D2A270, None),
    "base_hand_input_grip": (0x1019D80, None),
    "base_hand_constructor": (0x1000A50, None),
    "grip_value_exec_wrapper": (0x10BF580, None),
    "get_motion_controller_exec": (0x10C84C0, 32),
    "refresh_device_model": (0x2E9EF40, None),
    "show_device_model_exec": (0x2EB3010, None),
    "set_model_source_exec": (0x2EB2F20, None),
    "steamvr_render_model_name": (0x10ECFF0, None),
    "steamvr_create_model": (0x10EE560, None),
    "steamvr_async_model_loading": (0x10ED7C0, None),
}


def main():
    im = Image()
    actual_sha = hashlib.sha256(im.data).hexdigest()
    if actual_sha != EXPECTED_SHA:
        raise SystemExit("Executable fingerprint changed; re-trace before using these addresses.")
    out = Path(__file__).resolve().parents[1] / "artifacts/native-trace"
    out.mkdir(parents=True, exist_ok=True)
    for name, (rva, size) in FUNCTIONS.items():
        (out / f"{name}.asm").write_text(im.disasm(im.base+rva, size)+"\n")

    def pointer(va):
        return struct.unpack_from("<Q", im.data, im.offset(va))[0]

    # Verify independent links from Unreal registration metadata and vtables.
    assert pointer(0x144EC3A80) == 0x144EBE0B0  # GetVRControllerType name
    assert pointer(0x144EC3A88) == 0x1410D2BA0  # exec wrapper
    assert pointer(0x1451E3190+0x668) == 0x141D27220
    assert pointer(0x1451E3190+0x948) == 0x141D2A270
    assert pointer(0x144ED11F0+8) == 0x1410F3910
    call_rva = 0x1D27373
    off = im.offset(im.base+call_rva)
    original = im.data[off:off+5]
    assert original == bytes.fromhex("e8 88 b9 32 ff")
    assert im.base+call_rva+5+struct.unpack_from("<i",original,1)[0] == im.base+0x1052D00
    # A controlled Frame-only experiment can replace this one call's result.
    # Production needs a device-aware shim and normal fall-through for others.
    candidate = bytes.fromhex("b0 04 90 90 90")
    decoded = [f"{ins.mnemonic} {ins.op_str}".strip()
               for ins in im.cs.disasm(candidate, im.base+call_rva)]
    assert decoded == ["mov al, 4", "nop", "nop", "nop"]
    (out / "candidate-callsite.asm").write_text("\n".join(decoded)+"\n")
    branch_rva = 0x1D273D4
    branch_off = im.offset(im.base+branch_rva)
    assert im.data[branch_off:branch_off+2] == bytes.fromhex('75 73')
    branch = list(im.cs.disasm(im.data[branch_off:branch_off+2],im.base+branch_rva))
    assert len(branch) == 1 and branch[0].mnemonic == 'jne'
    assert branch[0].operands[0].imm == im.base+0x1D27449
    branch_candidate = bytes.fromhex('90 90')
    assert [i.mnemonic for i in im.cs.disasm(branch_candidate,im.base+branch_rva)] == ['nop','nop']
    callers = im.calls_to(im.base+0x1052D00)
    for call in callers:
        site = int(call['site'],16)
        target = int(call['target'],16)
        assert f'{site:#x}: call     {target:#x}' in im.disasm(site)
    report = {
        "status": "static_trace_verified",
        "executable_sha256": actual_sha,
        "preferred_image_base": hex(im.base),
        "functions": {name: {"rva": hex(rva), "preferred_va": hex(im.base+rva)}
                      for name, (rva,size) in FUNCTIONS.items()},
        "classifier_callers": callers,
        "index_enum": 4,
        "steamvr_unknown_fallback_enum": 2,
        "minimal_grip_experiment": {
            "scope": "hand input setup only, Frame sessions only",
            "branch_rva": hex(branch_rva),
            "branch_original_bytes": '75 73',
            "frame_only_branch_test_bytes": '90 90',
            "branch_effect": "Fall through to the existing Index binding branch; keep the original classifier call.",
            "call_rva": hex(call_rva),
            "original_bytes": original.hex(" "),
            "frame_only_test_bytes": candidate.hex(" "),
            "warning": "Offline candidate only. Requires device gating, early installation and runtime grip validation.",
            "apply_to_disk": False,
        },
        "hand_fields": {
            "motion_controller_component": "0x260",
            "left_right_selector": "0x7d4",
            "grip_press_threshold": "0xaf8",
            "grip_release_threshold": "0xafc",
            "knuckles_grip_press_threshold_property": "0xb00",
            "knuckles_grip_release_threshold_property": "0xb04",
            "grip_pressed": "0xe28",
            "grip_value": "0xe2c",
        },
        "cpp_constructor_defaults": {
            "grip_press": 0.5, "grip_release": 0.25,
            "knuckles_grip_press": struct.unpack("<f", bytes.fromhex("66 66 66 3f"))[0],
            "knuckles_grip_release": 0.25,
            "qualification": "Blueprint class defaults and runtime initialization can override these values.",
        },
        "representation": {
            "policy": "Preserve SteamVR identity and use its device model provider.",
            "active_profile_type_from_installed_profile": "frame_controller",
            "raw_models_in_2026_10_19_steamvr_log": ["Roy_Left", "Roy_Right"],
            "raw_render_models_in_2026_10_19_steamvr_log": [
                "{frame_controller}frame_controller_left",
                "{frame_controller}frame_controller_right",
            ],
            "motion_controller_show_model_field": "0xa85",
            "motion_controller_model_source_field": "0xa88",
            "qualification": "Engine display path exists. Game-side enabling, attachment and visibility need runtime verification.",
        },
    }
    (out / "trace.json").write_text(json.dumps(report,indent=2)+"\n")
    print(f"Verified registration, vtable and call-site links; wrote {len(FUNCTIONS)} disassemblies and trace.json.")


if __name__ == "__main__":
    main()
