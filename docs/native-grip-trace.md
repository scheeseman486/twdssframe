# Native implementation notes

The game classifies unmatched SteamVR devices as Vive. Frame therefore misses the existing Knuckles grip branch and inherits other Vive input rules. The mod selects Index behavior inside the game for recognized Frame controllers and supplies a native Frame SteamVR binding.

## Supported executable

Steam build: `10808074`

SHA256: `67615a629b1fd8b464b1d087166d91ba47b9427cd311ce665ca16eecc50df576`

Addresses below are RVAs from the loaded executable base. The preferred image base is `0x140000000`; ASLR changes actual addresses. Hooks require both the expected file hash and expected instruction bytes.

## Grip path

| Function or call | RVA | Behavior |
| --- | --- | --- |
| Native `GetVRControllerType` | `0x1052d00` | Returns Index enum 4 for Knuckles; unmatched SteamVR defaults to Vive enum 2. |
| Hand input setup | `0x1d27220` | Registers the selected grip axis delegate. |
| Classifier call in hand setup | `0x1d27373` | Return address `0x1d27378` identifies the scoped grip decision. |
| Game hand `InputGrip` | `0x1d2a270` | Applies gameplay restrictions, then calls base grip processing. |
| Base hand grip processing | `0x1019d80` | Stores the scalar and dispatches threshold transitions. |

Index hand setup selects `KnucklesLeftGrip` or `KnucklesRightGrip`. Both ordinary and Knuckles axes reach the same callback. The Frame binding supplies grip scalar values to those Knuckles actions. Runtime press/release thresholds were 0.5 and 0.25; no threshold tuning was required.

A scoped override was sufficient for grabbing. Full Index classification was subsequently required for the intended sprint behavior and help diagrams. Analog turning also has a Vive-specific gate at `0x1d18e40`. The classifier has 32 verified direct callers, so the full override deliberately applies game-wide.

The gate checks connected controllers against Frame type, model and render-model properties. Observed values were `frame_controller`, `Roy_Left`/`Roy_Right`, and `{frame_controller}frame_controller_left`/`frame_controller_right`. It scans registered controller identities independently of hand roles, including sleeping devices, so the initial setup can select Index before hand roles appear. The verified Frame mode persists across temporary disconnects. Connected non-Frame controllers prevent the override. Identity is cached for up to 250 ms. No OpenVR property is spoofed.

## Additional action bridge

The game exposes fewer SteamVR actions than its input configuration supports. The expanded manifest adds 48 action definitions. The bridge captures the game's own registered delegates during input setup and polls only added actions through the game's existing `IVRInput_007` interface.

| Hook or data | RVA |
| --- | --- |
| Character input setup | `0x1bfea60` |
| Player-controller input setup | `0x1d16040` |
| Bind action helper | `0xc80ce0` |
| Bind axis helper | `0xc80c00` |
| Game `IVRSystem_020` pointer | `0x627c300` |
| Game `IVRInput_007` pointer | `0x627c368` |

Captured UObject and input-component references are validated against object-array serial numbers and flags before delivery. Polling is throttled to at most once per 8 ms. Existing manifest actions remain handled by the game's SteamVR plugin. An exposed action without a registered handler receives no delivery.

## Reproduce the trace

Configure `paths.local.json`, install `requirements-trace.txt` in a disposable Python environment, then run:

```sh
python3 tools/trace_grip.py
```

The tool verifies the executable fingerprint, registration metadata, vtables and decoded instruction boundaries. Evidence is written under `artifacts/native-trace`. Windows unwind chains are followed to recover complete function ranges. The trace tool never patches the game.
