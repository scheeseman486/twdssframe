# SteamVR action catalog

The binding editor can assign game actions to any compatible public Frame input. Type compatibility still applies: button actions use button/trigger/D-pad modes; two-dimensional stick position uses vector2 actions.

The original plugin consumes original actions. The installed action bridge handles additional actions only when the current character/controller registers a matching callback. Context-dependent desktop actions and developer controls may have no VR handler. Their listing does not establish VR behavior. Hold sprint and hold crouch are confirmed by both user testing and delivered press/release telemetry; the other added context-dependent/developer actions remain unverified.

| Action | Type | Delivery/context |
| --- | --- | --- |
| controllerleft | pose | Original plugin |
| controllerright | pose | Original plugin |
| tracker_camera | pose | Original plugin |
| tracker_chest | pose | Original plugin |
| tracker_shoulder_left | pose | Original plugin |
| tracker_shoulder_right | pose | Original plugin |
| tracker_elbow_left | pose | Original plugin |
| tracker_elbow_right | pose | Original plugin |
| tracker_knee_left | pose | Original plugin |
| tracker_knee_right | pose | Original plugin |
| tracker_waist | pose | Original plugin |
| tracker_foot_left | pose | Original plugin |
| tracker_foot_right | pose | Original plugin |
| tracker_keyboard | pose | Original plugin |
| tracker_handed_pose_left | pose | Original plugin |
| tracker_handed_pose_right | pose | Original plugin |
| tracker_handed_back_left | pose | Original plugin |
| tracker_handed_back_right | pose | Original plugin |
| tracker_handed_front_left | pose | Original plugin |
| tracker_handed_front_right | pose | Original plugin |
| tracker_handed_frontr_left | pose | Original plugin |
| tracker_handed_frontr_right | pose | Original plugin |
| tracker_handed_grip_left | pose | Original plugin |
| tracker_handed_grip_right | pose | Original plugin |
| skeletonleft | skeleton | Original plugin |
| skeletonright | skeleton | Original plugin |
| vibrateleft | vibration | Original plugin |
| vibrateright | vibration | Original plugin |
| open_console | boolean | Original plugin |
| SprintToggle | boolean | Original plugin |
| SprintToggleMirror | boolean | Original plugin |
| TouchMovementActive | boolean | Original plugin |
| CrouchToggle | boolean | Original plugin |
| CrouchToggleMirror | boolean | Original plugin |
| ResetHMD | boolean | Original plugin |
| ResetHMDMirror | boolean | Original plugin |
| SelectDialogue | boolean | Original plugin |
| SelectDialogueMirror | boolean | Original plugin |
| Pause | boolean | Original plugin |
| PauseMirror | boolean | Original plugin |
| TurnAxisLeft | boolean | Original plugin |
| TurnAxisRight | boolean | Original plugin |
| ViveToggleUVFlashlight | boolean | Original plugin |
| ConfigurableTurnAxis axis | vector1 | Original plugin |
| ConfigurableTurnAxis_X,ConfigurableTurnAxis_Y X Y_axis2d | vector2 | Original plugin |
| MoveForward_X,MoveForward_Y X Y_axis2d | vector2 | Original plugin |
| ConfigurableTurnAxisMirror axis | vector1 | Original plugin |
| LeftInteract axis | vector1 | Original plugin |
| LeftInteractSecondary axis | vector1 | Original plugin |
| LeftGrip axis | vector1 | Original plugin |
| LeftGripIndex axis | vector1 | Original plugin |
| LeftGripMiddle axis | vector1 | Original plugin |
| LeftGripRing axis | vector1 | Original plugin |
| LeftGripPinky axis | vector1 | Original plugin |
| RightInteract axis | vector1 | Original plugin |
| RightInteractSecondary axis | vector1 | Original plugin |
| RightGrip axis | vector1 | Original plugin |
| RightGripIndex axis | vector1 | Original plugin |
| RightGripMiddle axis | vector1 | Original plugin |
| RightGripRing axis | vector1 | Original plugin |
| RightGripPinky axis | vector1 | Original plugin |
| SelectDialogueHorizontal axis | vector1 | Original plugin |
| SelectDialogueHorizontalMirror axis | vector1 | Original plugin |
| SelectDialogueVertical axis | vector1 | Original plugin |
| SelectDialogueVerticalMirror axis | vector1 | Original plugin |
| MoveForward axis | vector1 | Original plugin |
| MoveForwardMirror axis | vector1 | Original plugin |
| MoveRight axis | vector1 | Original plugin |
| MoveRightMirror axis | vector1 | Original plugin |
| Turn axis | vector1 | Original plugin |
| LookUp axis | vector1 | Original plugin |
| TurnAtRate axis | vector1 | Original plugin |
| LookUpAtRate axis | vector1 | Original plugin |
| KnucklesRightGrip axis | vector1 | Original plugin |
| KnucklesLeftGrip axis | vector1 | Original plugin |
| MenuClick axis | vector1 | Original plugin |
| NonVRInteractMoveForward axis | vector1 | Original plugin |
| NonVRInteractRotateYaw axis | vector1 | Original plugin |
| NonVRInteractRotatePitch axis | vector1 | Original plugin |
| NonVRInteractRotateYawAtRate axis | vector1 | Original plugin |
| NonVRInteractRotatePitchAtRate axis | vector1 | Original plugin |
| AxisEditOffsetX axis | vector1 | Original plugin |
| AxisEditOffsetY axis | vector1 | Original plugin |
| AxisEditOffsetZ axis | vector1 | Original plugin |
| AxisEditOffsetRoll axis | vector1 | Original plugin |
| AxisEditOffsetYaw axis | vector1 | Original plugin |
| AxisEditOffsetPitch axis | vector1 | Original plugin |
| KnucklesLeftPinkyGrip axis | vector1 | Original plugin |
| KnucklesRightPinkyGrip axis | vector1 | Original plugin |
| KnucklesLeftRingGrip axis | vector1 | Original plugin |
| KnucklesRightRingGrip axis | vector1 | Original plugin |
| KnucklesLeftMiddleGrip axis | vector1 | Original plugin |
| KnucklesRightMiddleGrip axis | vector1 | Original plugin |
| LeftIndexPointing axis | vector1 | Original plugin |
| RightIndexPointing axis | vector1 | Original plugin |
| LeftThumbUp axis | vector1 | Original plugin |
| RightThumbUp axis | vector1 | Original plugin |
| ADS | boolean | Bridge: context-dependent |
| CrouchHold | boolean | Bridge: VR |
| DecEditOffsetPitch | boolean | Bridge: developer |
| DecEditOffsetRoll | boolean | Bridge: developer |
| DecEditOffsetX | boolean | Bridge: developer |
| DecEditOffsetY | boolean | Bridge: developer |
| DecEditOffsetYaw | boolean | Bridge: developer |
| DecEditOffsetZ | boolean | Bridge: developer |
| DisplayCurrentEdit | boolean | Bridge: developer |
| EquipConsumable1 | boolean | Bridge: context-dependent |
| EquipConsumable2 | boolean | Bridge: context-dependent |
| EquipLarge | boolean | Bridge: context-dependent |
| EquipSmall | boolean | Bridge: context-dependent |
| GrabWorld | boolean | Bridge: context-dependent |
| IncEditOffsetPitch | boolean | Bridge: developer |
| IncEditOffsetRoll | boolean | Bridge: developer |
| IncEditOffsetX | boolean | Bridge: developer |
| IncEditOffsetY | boolean | Bridge: developer |
| IncEditOffsetYaw | boolean | Bridge: developer |
| IncEditOffsetZ | boolean | Bridge: developer |
| Interact | boolean | Bridge: context-dependent |
| InventoryBackpack | boolean | Bridge: context-dependent |
| InventoryFlashlight | boolean | Bridge: context-dependent |
| InventoryNotebook | boolean | Bridge: context-dependent |
| NonVRInteractCancel | boolean | Bridge: context-dependent |
| NonVRInteractConsume | boolean | Bridge: context-dependent |
| NonVRInteractDrop | boolean | Bridge: context-dependent |
| NonVRInteractInteract | boolean | Bridge: context-dependent |
| NonVRInteractPageLeft | boolean | Bridge: context-dependent |
| NonVRInteractPageRight | boolean | Bridge: context-dependent |
| NonVRInteractScrap | boolean | Bridge: context-dependent |
| NonVRInteractStore | boolean | Bridge: context-dependent |
| NonVRPause | boolean | Bridge: context-dependent |
| PrimaryAction | boolean | Bridge: context-dependent |
| Reload | boolean | Bridge: context-dependent |
| SecondaryAction | boolean | Bridge: context-dependent |
| SprintHold | boolean | Bridge: VR |
| StopEditing | boolean | Bridge: developer |
| Struggle | boolean | Bridge: context-dependent |
| UINavDown | boolean | Bridge: context-dependent |
| UINavLeft | boolean | Bridge: context-dependent |
| UINavRight | boolean | Bridge: context-dependent |
| UINavUp | boolean | Bridge: context-dependent |
| SprintHoldMirror | boolean | Bridge: VR alternate controls |
| CrouchHoldMirror | boolean | Bridge: VR alternate controls |
| TouchMovementActiveMirror | boolean | Bridge: VR alternate controls |
| TurnAxisLeftMirror | boolean | Bridge: VR alternate controls |
| TurnAxisRightMirror | boolean | Bridge: VR alternate controls |
