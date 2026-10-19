#!/usr/bin/env python3
"""Package the built mod as a drop-in release. No installed game or local paths needed."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.420.69'
INSTALL = '''TWD Saints & Sinners: Steam Frame support

1. In Steam, open the game's Properties > Installed Files > Browse.
2. Close the game and extract this archive into that folder.
3. Start the game through Steam with native Frame controller identity.

Proton only: add WINEDLLOVERRIDES="winmm=n,b" %command% to Steam launch options.
If you use gamescope: WINEDLLOVERRIDES="winmm=n,b" gamescope -- %command%

The mod installs its embedded Frame layout and extra actions automatically.
No scripts, compiler, Python, or manual binding file installation is needed.
If you have a personal/community layout selected, choose Default in SteamVR.
Do not replace an existing winmm.dll from another mod.

Supported: Steam build 10808074. Windows runtime testing is pending.
Logs: TWD/Binaries/Win64/TWDFrameGrip.log

To restore the original bindings, set RestoreBindings=1 in
TWD/Binaries/Win64/TWDFrameGrip.ini, launch once through Steam, then close it.
Remove TWDFrameGrip.asi, TWDFrameGrip.ini, winmm.dll and winmm.ini from that folder.
On Proton, remove the winmm launch option too.
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/windows')
    args = parser.parse_args()
    package = ROOT/'artifacts/package'
    if package.exists():
        # This directory is entirely generated; retire obsolete pre-0.4 JSON payloads.
        shutil.rmtree(package)
    package.mkdir(parents=True)
    shutil.copy2(args.build_dir/'TWDFrameGrip.asi', package/'TWDFrameGrip.asi')
    strip = shutil.which('x86_64-w64-mingw32-strip')
    if strip:
        subprocess.run([strip, '--strip-unneeded', str(package/'TWDFrameGrip.asi')], check=True)
    inspector = shutil.which('x86_64-w64-mingw32-objdump')
    if inspector:
        imports = subprocess.run([inspector, '-p', str(package/'TWDFrameGrip.asi')],
                                 check=True, capture_output=True, text=True).stdout
        dependencies = re.findall(r'DLL Name: (.+)', imports)
        external = [name for name in dependencies if name.lower().startswith(('libgcc', 'libstdc++', 'libwinpthread'))]
        if external:
            raise SystemExit('Build must embed compiler runtimes: ' + ', '.join(external))
    shutil.copy2(ROOT/'third_party/asi-loader/winmm.dll', package/'winmm.dll')
    (package/'TWDFrameGrip.ini').write_text('[FrameGrip]\nPatchEnabled=1\nIndexGameMode=1\nFullActions=1\n')
    (package/'winmm.ini').write_text('[GlobalSets]\nDontLoadFromDllMain=1\nForceEntryPointHook=1\n')
    hashes = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in package.iterdir()}
    (package/'SHA256.json').write_text(json.dumps(hashes, indent=2)+'\n')
    releases = ROOT/'artifacts/releases'
    releases.mkdir(parents=True, exist_ok=True)
    archive = releases/f'twdssframe-{VERSION}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for name in hashes:
            output.write(package/name, 'TWD/Binaries/Win64/'+name)
        output.writestr('TWDFrameGrip-INSTALL.txt', INSTALL)
        output.write(package/'SHA256.json', 'TWDFrameGrip-licenses/SHA256.json')
        output.write(ROOT/'LICENSE', 'TWDFrameGrip-licenses/project-MIT.txt')
        for name, path in [('MinHook','minhook/LICENSE.txt'), ('OpenVR','openvr/LICENSE'),
                           ('Ultimate-ASI-Loader','asi-loader/LICENSE'), ('picojson','picojson/LICENSE')]:
            output.write(ROOT/'third_party'/path, 'TWDFrameGrip-licenses/'+name+'.txt')
    print('Prepared', archive)
    print('Native module:', (package/'TWDFrameGrip.asi').stat().st_size, 'bytes; bindings embedded.')

if __name__ == '__main__':
    main()
