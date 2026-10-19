#!/usr/bin/env python3
"""Prepare a reversible local test package; never writes to game/Steam folders."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    paths=json.loads((ROOT/'paths.local.json').read_text())
    game=Path(paths['game_root'])
    package=ROOT/'artifacts/package'
    package.mkdir(parents=True,exist_ok=True)
    for source,target in [
        (ROOT/'build/windows/TWDFrameGrip.asi','TWDFrameGrip.asi'),
        (ROOT/'third_party/asi-loader/winmm.dll','winmm.dll'),
    ]:
        shutil.copy2(source,package/target)
    subprocess.run(['x86_64-w64-mingw32-strip','--strip-unneeded',str(package/'TWDFrameGrip.asi')],check=True)
    (package/'TWDFrameGrip.ini').write_text(
        '[FrameGrip]\nPatchEnabled=1\nIndexGameMode=1\nFullActions=1\n')
    (package/'winmm.ini').write_text('[GlobalSets]\nDontLoadFromDllMain=1\nForceEntryPointHook=1\n')
    binding_root=game/'TWD/Config/SteamVRBindings'
    # Build a Frame-native default without copying an Index preset.
    binding={'action_manifest_version':0,'app_key':'steam.app.916840',
             'category':'steamvr_input','controller_type':'frame_controller',
             'name':'TWD Saints & Sinners - Frame',
             'description':'Native Frame controls with Index game behavior and fully editable game actions.',
             'options':{},'alias_info':{},'simulated_actions':[]}
    transformed=[]
    def source(side,control,mode,inputs,parameters=None):
        item={'path':f'/user/hand/{side}/input/{control}','mode':mode,
              'inputs':{key:{'output':'/actions/main/in/'+action} for key,action in inputs.items()}}
        if parameters:item['parameters']=parameters
        transformed.append(item)
    for side in ('left','right'):
        source(side,'grip','trigger',{'pull':'Knuckles'+side.title()+'Grip axis'})
        source(side,'trigger','trigger',{'pull':side.title()+'Interact axis','touch':side.title()+'IndexPointing axis'})
        source(side,'bumper','button',{'click':side.title()+'InteractSecondary axis'})
    source('left','thumbstick','joystick',{'position':'MoveForward_X,MoveForward_Y X Y_axis2d',
                                         'click':'SprintToggle'},{'invert':'y'})
    source('right','thumbstick','joystick',{'position':'ConfigurableTurnAxis_X,ConfigurableTurnAxis_Y X Y_axis2d',
                                          'click':'ResetHMD'})
    source('right','thumbstick','dpad',{'south':'CrouchToggle'},
           {'sub_mode':'touch','deadzone_pct':'50','overlap_pct':'0'})
    for side,control,action in [('left','view','Pause'),('right','menu','Pause'),
                               ('left','dpad_left','SelectDialogue'),('right','x','SelectDialogue'),
                               ('left','dpad_down','LeftInteractSecondary axis'),
                               ('right','a','RightInteractSecondary axis'),('right','b','CrouchToggle')]:
        source(side,control,'button',{'click':action})
    # Keep additional public Frame buttons visible as unused editable sources.
    for side,control in [('left','dpad_right'),('left','dpad_up'),('right','y')]:
        source(side,control,'button',{})
    binding['bindings']={'/actions/main':{
        'sources':transformed,
        'poses':[{'path':f'/user/hand/{side}/pose/raw','output':f'/actions/main/in/controller{side}'}
                 for side in ('left','right')],
        'haptics':[{'path':f'/user/hand/{side}/output/haptic','output':f'/actions/main/out/vibrate{side}'}
                   for side in ('left','right')],
        'skeleton':[{'path':f'/user/hand/{side}/input/skeleton/{side}','output':f'/actions/main/in/skeleton{side}'}
                    for side in ('left','right')]}}
    binding_name='frame_controller_TWDFrameGrip.json'
    manifest=json.loads((binding_root/'steamvr_manifest.json').read_text())
    additional=json.loads((ROOT/'config/additional_actions.json').read_text())
    existing={entry['name'].lower() for entry in manifest['actions']}
    labels={}
    for entry in additional:
        path='/actions/main/in/'+entry['action']
        if path.lower() not in existing:
            manifest['actions'].append({'name':path,'type':entry['type'],'requirement':'optional'})
            existing.add(path.lower())
        labels[path]=entry['name']+' ['+entry['context']+']'
    localization=manifest.setdefault('localization',[])
    english=next((row for row in localization if row.get('language_tag')=='en_us'),None)
    if english is None:
        english={'language_tag':'en_us'};localization.append(english)
    english.update(labels)
    (ROOT/'src/additional_actions.inc').write_text(
        '// Generated from config/additional_actions.json by prepare_package.py.\n'+
        ''.join('{L'+json.dumps(entry['name'])+','+json.dumps('/actions/main/in/'+entry['action'])+','+
                ('true' if entry['type']=='vector1' else 'false')+'},\n' for entry in additional))
    actions={action['name'].lower():action['name'] for action in manifest['actions']}
    # Reject every stale output, including pose/haptic/skeleton outputs, before
    # installation. Preserve exact manifest spelling for the generated binding.
    def validate_outputs(node):
        if isinstance(node,dict):
            for key,value in node.items():
                if key=='output':
                    assert value.lower() in actions,('Unknown output',value)
                    node[key]=actions[value.lower()]
                else:validate_outputs(value)
        elif isinstance(node,list):
            for value in node:validate_outputs(value)
    validate_outputs(binding['bindings'])
    (package/binding_name).write_text(json.dumps(binding,indent=2)+'\n')
    manifest['default_bindings']=[x for x in manifest['default_bindings'] if x['controller_type']!='frame_controller']
    manifest['default_bindings'].insert(0,{'controller_type':'frame_controller','binding_url':binding_name})
    (package/'steamvr_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in package.iterdir() if p.is_file() and p.name!='SHA256.json'}
    (package/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    # Verify every physical source and requested grip action against the installed profiles.
    profile=json.loads((Path(paths['steamvr_root'])/'drivers/frame_controller/resources/input/frame_controller_profile.json').read_text())
    supported=profile['input_source']
    for source in transformed:
        path=source['path'];component='/'+path.split('/user/hand/')[1].split('/',1)[1]
        assert component in supported,component
        side=path.split('/')[3]
        assert supported[component].get('side',side)==side,(path,supported[component])
    grips=[x for x in transformed if x['path'].endswith('/input/grip')]
    assert len(grips)==2 and all(x['mode']=='trigger' for x in grips)
    assert not any('simulat' in k for k in binding['options'])
    print('Prepared',package,'with verified native Frame input paths; no emulation options.')

if __name__=='__main__':main()
