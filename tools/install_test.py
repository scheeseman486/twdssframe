#!/usr/bin/env python3
"""Install/restore this test's exact files with backups and conflict detection."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

ROOT=Path(__file__).resolve().parents[1]
EXPECTED='67615a629b1fd8b464b1d087166d91ba47b9427cd311ce665ca16eecc50df576'

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['install','update-binding','update-mod','restore'])
    args=parser.parse_args()
    game=Path(json.loads((ROOT/'paths.local.json').read_text())['game_root'])
    binary=game/'TWD/Binaries/Win64'
    bindings=game/'TWD/Config/SteamVRBindings'
    backup=ROOT/'artifacts/install-backup'
    state_path=backup/'state.json'
    if args.action in ('update-binding','update-mod'):
        state=json.loads(state_path.read_text())
        if state.get('restored'):raise SystemExit('The test installation was restored.')
        if digest(binary/'TWD-Win64-Shipping.exe')!=EXPECTED:raise SystemExit('Game build fingerprint changed.')
        names=['frame_controller_TWDFrameGrip.json']
        if args.action=='update-mod':names+=['TWDFrameGrip.asi','TWDFrameGrip.ini','steamvr_manifest.json']
        package=ROOT/'artifacts/package'
        package_hashes=json.loads((package/'SHA256.json').read_text())
        changes=[]
        for name in names:
            dest=(bindings if name.endswith('.json') else binary)/name
            entry=next(x for x in state['files'] if x['path']==str(dest))
            if digest(dest)!=entry['installed_sha256']:
                raise SystemExit(f'Installed file changed; refusing to overwrite: {dest}')
            new_hash=digest(package/name)
            if new_hash!=package_hashes[name]:raise SystemExit(f'Prepared hash mismatch: {name}')
            if new_hash==entry['installed_sha256']:continue
            prior=dest.read_bytes()
            (backup/f'before-{entry["installed_sha256"]}-{name}').write_bytes(prior)
            temporary=dest.with_name(dest.name+'.twdframegrip.tmp')
            if temporary.exists():raise SystemExit(f'Review stale temporary file first: {temporary}')
            changes.append((entry,dest,package/name,temporary,prior,new_hash))
        written=[]
        try:
            for entry,dest,source,temporary,prior,new_hash in changes:
                shutil.copy2(source,temporary);temporary.replace(dest);written.append((dest,prior))
                entry['installed_sha256']=new_hash
            state_path.write_text(json.dumps(state,indent=2)+'\n')
        except Exception:
            for dest,prior in reversed(written):dest.write_bytes(prior)
            for _,_,_,temporary,_,_ in changes:temporary.unlink(missing_ok=True)
            raise
        print('Updated',len(changes),'test files with hash-checked backups.');return
    if args.action=='restore':
        state=json.loads(state_path.read_text())
        # Validate all files before changing any of them.
        for entry in state['files']:
            dest=Path(entry['path'])
            if not dest.exists() or digest(dest)!=entry['installed_sha256']:
                raise SystemExit(f'Refusing to overwrite changed/missing file: {dest}')
        for entry in reversed(state['files']):
            dest=Path(entry['path'])
            if entry['existed']:shutil.copy2(backup/entry['backup_name'],dest)
            else:dest.unlink()
        state['restored']=True;state_path.write_text(json.dumps(state,indent=2)+'\n')
        print('Restored all original installation files.');return
    if state_path.exists():raise SystemExit('An installation record already exists; inspect/restore it before installing again.')
    if digest(binary/'TWD-Win64-Shipping.exe')!=EXPECTED:raise SystemExit('Game build fingerprint changed.')
    package=ROOT/'artifacts/package'
    targets=[(name,binary/name) for name in ['winmm.dll','winmm.ini','TWDFrameGrip.asi','TWDFrameGrip.ini']]
    targets += [(name,bindings/name) for name in ['frame_controller_TWDFrameGrip.json','steamvr_manifest.json']]
    for name,dest in targets:
        if not (package/name).is_file():raise SystemExit(f'Missing package file: {name}')
        if dest.exists() and name!='steamvr_manifest.json':raise SystemExit(f'Existing mod/config would be overwritten: {dest}')
    backup.mkdir(parents=True,exist_ok=True)
    state={'game_root':str(game),'restored':False,'files':[]}
    for index,(name,dest) in enumerate(targets):
        entry={'path':str(dest),'existed':dest.exists(),'backup_name':f'{index}-{name}',
               'installed_sha256':digest(package/name)}
        if entry['existed']:shutil.copy2(dest,backup/entry['backup_name'])
        state['files'].append(entry)
    # Persist recovery metadata before the first external write.
    state_path.write_text(json.dumps(state,indent=2)+'\n')
    copied=[]
    try:
        for name,dest in targets:
            shutil.copy2(package/name,dest);copied.append(dest)
    except Exception:
        for entry in reversed(state['files'][:len(copied)]):
            dest=Path(entry['path'])
            if entry['existed']:shutil.copy2(backup/entry['backup_name'],dest)
            else:dest.unlink(missing_ok=True)
        raise
    print('Installed loader, native hooks and Frame default binding; original manifest backed up.')

if __name__=='__main__':main()
