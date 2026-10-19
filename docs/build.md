# Developer build

End users should install the release ZIP. These commands are for building it on Linux with CMake, Ninja, Python 3 and the MinGW Windows x64 C++ compiler:

```sh
cmake -S . -B build/windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows
python3 tools/prepare_package.py
```

The archive is written to `artifacts/releases`. Building and packaging require no installed game, SteamVR or `paths.local.json`. The Frame layout and additional actions are embedded from `config/frame_binding.json` and `config/additional_actions.json`.

Native checks:

```sh
g++ -std=c++17 -Wall -Wextra -Isrc tests/frame_policy_test.cpp -o build/frame_policy_test
build/frame_policy_test
g++ -std=c++17 -Wall -Wextra -Isrc -Ithird_party/picojson \
  -Ibuild/windows/generated tests/binding_manifest_test.cpp -o build/binding_manifest_test
build/binding_manifest_test
```

The inspection and disassembly tools still use `paths.local.json`; copy `paths.example.json` and configure it only when using those tools. `requirements-trace.txt` contains their optional Python dependencies. `tools/install_test.py` manages local test deployments with hash-checked backups.
