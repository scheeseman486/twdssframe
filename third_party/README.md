# Dependencies

The module statically includes MinHook 1.3.4 and compiles against Valve's OpenVR 1.5.17 header, matching the game's `IVRSystem_020` interface. Their source headers and license files are preserved in this directory.

Ultimate ASI Loader 9.7.4 supplies the dedicated Windows x64 winmm bootstrap. `asi-loader/SOURCE.txt` records its download URL and SHA256. Its license is preserved beside the binary. The current binary version was verified from the PE version resource.

Sources: [MinHook](https://github.com/TsudaKageyu/minhook/tree/v1.3.4), [OpenVR](https://github.com/ValveSoftware/openvr/tree/v1.5.17), [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/v9.7.4).

picojson 1.3.0 parses and merges the installed action manifest at startup. Its BSD license is preserved in `picojson/LICENSE` and the header. Source: [picojson 1.3.0](https://github.com/kazuho/picojson/tree/v1.3.0).
