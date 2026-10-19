#!/usr/bin/env python3
"""Set/restore the game-specific native winmm override while the game is closed.

Edits the existing Proton prefix registry file; never executes Wine or Proton.
Rollback changes only this value and preserves unrelated registry updates.
"""
from pathlib import Path
import argparse
import json
import os
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
SECTION=r'Software\\Wine\\AppDefaults\\TWD-Win64-Shipping.exe\\DllOverrides'
VALUE='native,builtin'
HEADER=re.compile(r'^\['+re.escape(SECTION)+r'\][^\n]*(?:\n|$)',re.M|re.I)
ENTRY=re.compile(r'^"winmm"\s*=\s*"([^"\n]*)"[^\n]*(?:\n|$)',re.M|re.I)

def locate(text):
    header=HEADER.search(text)
    if not header:return None
    next_section=re.search(r'^\[',text[header.end():],re.M)
    end=header.end()+next_section.start() if next_section else len(text)
    return header.start(),header.end(),end

def write_atomic(path,text):
    temporary=path.with_name(path.name+'.twdframegrip.tmp')
    if temporary.exists():raise SystemExit(f'Review stale temporary file first: {temporary}')
    with temporary.open('x') as handle:
        handle.write(text);handle.flush();os.fsync(handle.fileno())
    shutil.copymode(path,temporary)
    os.replace(temporary,path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['set','restore'])
    args=parser.parse_args()
    game=Path(json.loads((ROOT/'paths.local.json').read_text())['game_root'])
    path=game.parent.parent/'compatdata/916840/pfx/user.reg'
    state_path=ROOT/'artifacts/install-backup/proton-override.json'
    text=path.read_text();bounds=locate(text)
    previous=ENTRY.search(text[bounds[1]:bounds[2]]) if bounds else None
    current=previous.group(1) if previous else None
    if args.action=='set':
        if state_path.exists():raise SystemExit('A Proton override recovery record already exists.')
        state={'path':str(path),'previous':current,'section_existed':bounds is not None,
               'installed':VALUE,'restored':False}
        state_path.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,state_path.parent/'user.reg.before-frame-grip')
        state_path.write_text(json.dumps(state,indent=2)+'\n')
        line=f'"winmm"="{VALUE}"\n'
        if bounds:
            a,b,c=bounds;body=text[b:c]
            body=ENTRY.sub(lambda m:line,body,count=1) if previous else line+body
            text=text[:b]+body+text[c:]
        else:text=text.rstrip()+f'\n\n[{SECTION}]\n'+line+'\n'
    else:
        state=json.loads(state_path.read_text())
        if state.get('restored'):raise SystemExit('Proton override already restored.')
        if not bounds or current!=state['installed']:raise SystemExit('Override changed; refusing to overwrite it.')
        a,b,c=bounds
        replacement='' if state['previous'] is None else f'"winmm"="{state["previous"]}"\n'
        body=ENTRY.sub(lambda m:replacement,text[b:c],count=1)
        meaningful=[line for line in body.splitlines() if line.strip() and not line.startswith('#')]
        if not state['section_existed'] and not meaningful:text=text[:a]+text[c:]
        else:text=text[:b]+body+text[c:]
    write_atomic(path,text)
    if args.action=='restore':
        state['restored']=True;state_path.write_text(json.dumps(state,indent=2)+'\n')
    print('Game-specific Proton winmm override', 'set; Steam launch options unchanged.' if args.action=='set' else 'restored.')

if __name__=='__main__':main()
