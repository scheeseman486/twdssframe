#!/usr/bin/env python3
"""Embed the mod's own binding and extra action definitions. No game files needed."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def generate(output):
    binding = json.loads((ROOT / 'config/frame_binding.json').read_text())
    actions = json.loads((ROOT / 'config/additional_actions.json').read_text())
    output.mkdir(parents=True, exist_ok=True)
    text = '#pragma once\nnamespace embedded {\n'
    for name, value in [('binding', binding), ('actions', actions)]:
        value = json.dumps(value, separators=(',', ':'), ensure_ascii=True)
        text += 'inline constexpr char ' + name + '[] = ' + json.dumps(value) + ';\n'
    text += '}\n'
    target = output / 'embedded_bindings.h'
    if not target.exists() or target.read_text() != text:
        target.write_text(text)
    delegates = '// Generated from config/additional_actions.json.\n'
    delegates += ''.join('{L'+json.dumps(a['name'])+','+
                         json.dumps('/actions/main/in/'+a['action'])+','+
                         ('true' if a['type']=='vector1' else 'false')+'},\n' for a in actions)
    target = output / 'additional_actions.inc'
    if not target.exists() or target.read_text() != delegates:
        target.write_text(delegates)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    generate(parser.parse_args().output)
