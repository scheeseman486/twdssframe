# TWD Saints & Sinners: Frame support

Steam Frame controller support using the game's existing Index input behavior. Restores grabbing, movement and sprint, and exposes additional actions for rebinding. Frame keeps its real SteamVR identity.

## Install

Download the install ZIP from the [latest release](https://github.com/scheeseman486/twdssframe/releases/latest), then:

1. In Steam, open **Properties > Installed Files > Browse** for the game.
2. Extract the release ZIP into that folder.
3. Start the game through Steam with native Frame controller identity selected.

The mod supplies its own bindings automatically. If a personal/community layout is selected, choose **Default** in SteamVR controller bindings.

**Linux/Proton only:** set Steam launch options to:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

If using gamescope (I've found it helps with stability, YMMV), keep it before `%command%`: `WINEDLLOVERRIDES="winmm=n,b" gamescope -- %command%`.

Supports Steam build **10808074**. Do not overwrite an existing `winmm.dll` from another mod. Windows runtime testing is pending, let me know how it goes.

## Default controls

| Control | Action |
| --- | --- |
| Left stick / click | Move / sprint toggle |
| Right stick left or right / click | Turn / reset HMD |
| Right stick down or right B | Crouch toggle |
| Grips / triggers | Grab and release / primary interaction |
| Bumpers, left D-pad down or right A | Secondary interaction |
| Left D-pad left or right X | Dialogue selection |
| Left View or right Menu | Pause |

Right Y, left D-pad up and left D-pad right are unassigned. Hold sprint and hold crouch remain available for rebinding. Additional actions depend on handlers provided by the current game context. See the [action catalog](docs/action-catalog.md).

## Remove

Set `RestoreBindings=1` under `[FrameGrip]` in `TWD/Binaries/Win64/TWDFrameGrip.ini`. Launch once through Steam, then close the game. Delete `TWDFrameGrip.asi`, `TWDFrameGrip.ini`, `winmm.dll` and `winmm.ini` from that folder. On Proton, remove the `winmm` launch setting too.

The mod backs up the original manifest and any existing Frame default before changing them. It refuses to overwrite configuration changed since its last update.

[Build from source](docs/build.md) · [Settings and logs](docs/runtime-test.md) · [Implementation notes](docs/native-grip-trace.md)

Project code is [MIT licensed](LICENSE). Dependencies retain their [own licenses](third_party/README.md).
