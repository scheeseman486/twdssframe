# Runtime settings and verification

Launch through Steam. On Proton, use `WINEDLLOVERRIDES="winmm=n,b" %command%`, preserving any wrapper before `%command%`. Select native Frame identity and the default **TWD Saints & Sinners - Frame** layout in SteamVR.

The manifest registers this layout for `frame_controller`. A saved personal or community binding can override it. Choose Default to return to the supplied layout. Reload Default after installing a layout update, or restart the game.

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
| `BUILD_VERIFIED`, `HOOKS_READY` | Executable verification and hook setup succeeded. |
| `HAND_SETUP` | Actual controller properties and Frame recognition. |
| `GAME_CONTROLLER_MODE` | Frame-only Index game behavior selected. |
| `GRIP` | Scalar values, thresholds and press/release transitions. |
| `EXTRA_BIND`, `EXTRA_HANDLE` | Added action callbacks captured and handles resolved. |
| `EXTRA_INPUT` | Added action transition; `delivered=1` means a valid handler received it. |
| `REFUSED`, `HOOK_ENABLE_FAILED` | Check the logged reason before testing further. |

## Verified behavior

Steam/Proton testing on 2026-10-19 confirmed grabbing and releasing with both hands, movement, turning, crouch, single-click sprint and Index help diagrams. Hold sprint and hold crouch also worked through the additional action bridge, with logged press and release delivery. Their temporary default buttons were subsequently cleared.

Other context-dependent and developer actions have not all been tested. They work only where the game registers a matching handler. Windows runtime behavior remains untested.

The module verifies the entire executable SHA256 and hook bytes. It changes code in memory only. Index mode requires at least one connected Frame controller and rejects a mixed set containing non-Frame controllers. OpenVR identity and render-model properties remain untouched.

## Recovery

Close the game before replacing modules or restoring files. `python3 tools/install_test.py restore` restores the original manifest and removes installed mod files. It checks hashes and refuses to overwrite files changed since installation. Recovery data stays in `artifacts/install-backup`.

Remove the `WINEDLLOVERRIDES` launch setting after removal. If the optional prefix override was used, restore it with `python3 tools/proton_override.py restore`. That tool edits only the app-specific value and never launches Wine or Proton.
