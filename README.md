# TWD Saints & Sinners: Frame support

A small native mod that enables Steam Frame controllers using the game's existing Index support. Frame keeps its real SteamVR identity. The mod restores grabbing, enables Index input behavior and exposes additional game actions for rebinding.

Tested through Steam/Proton: grabbing and releasing with both hands, movement, turning, crouch, sprint toggle, hold sprint, hold crouch and Index help diagrams. Windows has not been tested.

## Install

Supports Steam build **10808074** only. Other executable versions are refused. Close the game before installing.

1. Copy `paths.example.json` to `paths.local.json` and set the game and SteamVR folders.
2. Build using the commands below, then run `python3 tools/prepare_package.py`.
3. Run `python3 tools/install_test.py install`. It backs up the original manifest and refuses to overwrite existing mods.
4. On Proton, add `WINEDLLOVERRIDES="winmm=n,b" %command%` to the game's Steam launch options. If using gamescope, use `WINEDLLOVERRIDES="winmm=n,b" gamescope -- %command%`.
5. Start through Steam with both controllers awake and their native Frame identity selected.

**TWD Saints & Sinners - Frame** is registered as the default layout for Frame controllers. If SteamVR has a personal or community binding selected, choose **Default** once. Personal bindings remain editable and take precedence over later default updates.

## Default controls

| Control | Action |
| --- | --- |
| Left stick / click | Move / sprint toggle |
| Right stick left or right / click | Turn / reset HMD |
| Right stick down or right B | Crouch toggle |
| Grips | Grab and release |
| Triggers | Primary interaction |
| Bumpers, left D-pad down or right A | Secondary interaction |
| Left D-pad left or right X | Dialogue selection |
| Left View or right Menu | Pause |

Right Y, left D-pad up and left D-pad right are unassigned. Hold sprint and hold crouch remain available in SteamVR's binding editor. All public Frame controls can be rebound; SteamVR reserves some system inputs. Additional actions depend on the handlers the current game context provides. See the [action catalog](docs/action-catalog.md).

## Build

Requires CMake, Ninja and the MinGW Windows x64 C++ compiler. No Unreal Editor or runtime mod framework is required.

```sh
cmake -S . -B build/windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows
```

Policy checks run natively on Linux:

```sh
g++ -std=c++17 -Wall -Wextra -Isrc tests/frame_policy_test.cpp -o build/frame_policy_test
build/frame_policy_test
```

The ASI module is about 96 KB. [Ultimate ASI Loader](third_party/README.md) provides the separate `winmm.dll` bootstrap. Package preparation reads the installed game's action manifest and SteamVR Frame profile; game files, logs and local paths stay outside Git.

## Update and remove

After regenerating the package, use `python3 tools/install_test.py update-binding` for layout changes. Choose Default again in SteamVR to reload it. For module updates, close the game and use `python3 tools/install_test.py update-mod`.

To remove, close the game and run `python3 tools/install_test.py restore`, then remove the launch option's `WINEDLLOVERRIDES` setting. If you used the optional registry override tool, run `python3 tools/proton_override.py restore` too. Keep `artifacts/install-backup` until removal is complete.

[Runtime settings and logs](docs/runtime-test.md) · [Native implementation notes](docs/native-grip-trace.md)

Project code is [MIT licensed](LICENSE). Dependencies retain their [own licenses](third_party/README.md). No game binaries or assets are included.
