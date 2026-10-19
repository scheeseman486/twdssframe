# Runtime settings and verification

Launch through Steam. On Proton, use `WINEDLLOVERRIDES="winmm=n,b" %command%`, preserving any wrapper before `%command%`. Select native Frame identity and the default **TWD Saints & Sinners - Frame** layout in SteamVR.

The manifest registers this layout for `frame_controller`. A saved personal or community binding can override it. Choose Default to return to the supplied layout. The DLL embeds the Frame layout and additional actions. On startup, after executable verification and before game input setup, it merges them into the game manifest at its Steam-registered path. Originals and SHA256 recovery metadata are retained in the bindings folder. Updates preserve unrelated actions, controller defaults and translations. Invalid JSON, conflicting action types, or unexpected changes stop installation and hook setup.

After updating the mod, restart the game and reload Default if necessary.

## Settings

`TWD/Binaries/Win64/TWDFrameGrip.ini` accepts:

```ini
[FrameGrip]
PatchEnabled=1
IndexGameMode=1
FullActions=1
```

`PatchEnabled=0` disables classifier overrides and added action delivery. `IndexGameMode=0` limits the override to hand grip setup. `FullActions=0` disables the bridge for additional actions. Restart after changing settings.

Logs default to `TWD/Binaries/Win64/TWDFrameGrip.log`. Set `LogDirectory` to an existing Windows path to override that location. Under Proton, Linux paths can be reached through `Z:\`. Each session uses UTC timestamps.

| Log entry | Meaning |
| --- | --- |
| `BINDINGS_READY` | Embedded bindings were installed or verified unchanged. |
| `BINDINGS_REFUSED` | Configuration could not be safely updated; hooks are not enabled. |
| `BUILD_VERIFIED`, `HOOKS_READY` | Executable verification and hook setup succeeded. |
| `HAND_SETUP` | Actual controller properties and Frame recognition. |
| `GAME_CONTROLLER_MODE` | Frame-only Index game behavior selected. |
| `GRIP` | Scalar values, thresholds and press/release transitions. |
| `EXTRA_BIND`, `EXTRA_HANDLE` | Added action callbacks captured and handles resolved. |
| `EXTRA_INPUT` | Added action transition; `delivered=1` means a valid handler received it. |
| `REFUSED`, `HOOK_ENABLE_FAILED` | Check the logged reason before testing further. |

`RestoreBindings=1` restores the original configuration on the next Steam launch and skips all hooks. Close the game after that launch and remove the mod files.

## Verified behavior

Steam/Proton testing on 2026-10-19 confirmed grabbing and releasing with both hands, movement, turning, crouch, single-click sprint and Index help diagrams. Hold sprint and hold crouch also worked through the additional action bridge, with logged press and release delivery. Their temporary default buttons were subsequently cleared.

The 0.4.0 clean-install test on 2026-10-20 began with the original manifest restored and the old Frame binding and added actions absent. Only the loader, module and settings were copied. The DLL created the Frame layout, added 48 actions, preserved the original controller defaults and retained an exact original-manifest backup. SteamVR loaded the generated default successfully.

The first test exposed delayed recognition when hand roles were unavailable. Recognition now scans registered controller identities, including sleeping devices, before the first classifier decision. In the corrected session, the first 16 logged classifier results all selected Index, and both hands selected Knuckles grip axes during their initial setup. The user confirmed correct cursor direction from startup and working controls.

Other context-dependent and developer actions have not all been tested. They work only where the game registers a matching handler. Windows runtime behavior remains untested.

The module verifies the entire executable SHA256 and hook bytes. It changes code in memory only. Index mode recognizes registered Frame identities before hand-role assignment, including sleeping controllers. Once verified, the session retains Frame mode across temporary disconnects. A connected non-Frame controller still prevents the override. OpenVR identity and render-model properties remain untouched.

## Recovery

Automatic binding backups live in `TWD/Config/SteamVRBindings` as `TWDFrameGrip.original-manifest.json` and, when applicable, `TWDFrameGrip.original-binding.json`. `TWDFrameGrip.binding-state.json` records hashes and incomplete transactions. Do not delete these while the mod is installed. `RestoreBindings=1` validates and restores them before removing the active recovery record.

The developer installer has separate recovery data in `artifacts/install-backup`. If the optional Proton registry override was used during development, restore it with `python3 tools/proton_override.py restore`. End users only need the launch option.
