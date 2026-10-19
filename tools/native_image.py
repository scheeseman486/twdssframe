"""Small read-only PE disassembly helper for the controller investigation.

Requires pefile and capstone. Addresses are preferred-image VAs, before ASLR.
Call/ref scans return candidates; verify instruction boundaries in disassembly.
"""
import bisect
import json
from pathlib import Path
import re
import struct

import capstone
import pefile


class Image:
    def __init__(self, path=None):
        if path is None:
            config = json.loads((Path(__file__).resolve().parents[1] / 'paths.local.json').read_text())
            path = Path(config['game_root']) / 'TWD/Binaries/Win64/TWD-Win64-Shipping.exe'
        self.data = Path(path).read_bytes()
        self.pe = pefile.PE(data=self.data, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        pdata = next(s for s in self.pe.sections if s.Name.startswith(b'.pdata'))
        raw = pdata.get_data()
        entries = [struct.unpack_from('<III', raw, i)
                   for i in range(0, len(raw)-11, 12)]
        entries = [(a,b,u) for a,b,u in entries if a and b>a]
        self.owners = {}
        self.groups = {}
        for a,b,u in entries:
            root = a
            seen = set()
            while u not in seen:
                seen.add(u)
                off = self.pe.get_offset_from_rva(u)
                header = self.data[off:off+4]
                if not (header[0] >> 3 & 4):
                    break
                chain = off+4+((header[2]+1)//2)*4
                root,_,u = struct.unpack_from('<III',self.data,chain)
            owner = root+self.base
            self.owners[a+self.base] = owner
            self.groups.setdefault(owner,[]).append((a+self.base,b+self.base))
        self.functions = sorted((a+self.base, b+self.base) for a,b,u in entries)
        self.starts = [a for a,b in self.functions]
        self.cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.cs.detail = True
        self.text = next(s for s in self.pe.sections if s.Name.startswith(b'.text'))

    def offset(self, va):
        return self.pe.get_offset_from_rva(va-self.base)

    def va(self, offset):
        return self.base+self.pe.get_rva_from_offset(offset)

    def bounds(self, va):
        i = bisect.bisect_right(self.starts,va)-1
        if i>=0 and self.functions[i][0]<=va<self.functions[i][1]:
            owner = self.owners[self.functions[i][0]]
            return owner,max(b for a,b in self.groups[owner])
        return (va,va+0x100)

    def describe(self, va):
        try:
            off=self.offset(va)
            b=self.data[off:off+180]
            a=b.split(b'\0')[0]
            if len(a)>=4 and all(32<=c<127 for c in a):
                return repr(a.decode())
            if len(b)>3 and b[1]==0:
                s=b.decode('utf-16-le',errors='replace').split('\0')[0]
                if len(s)>=3 and all(32<=ord(c)<127 for c in s):
                    return repr(s)
        except (ValueError,pefile.PEFormatError):
            pass
        return ''

    def disasm(self, va, size=None):
        start,end=self.bounds(va) if size is None else (va,va+size)
        lines=[f'Function {start:#x}..{end:#x} (RVA {start-self.base:#x})']
        ranges = sorted(self.groups.get(start,[(start,end)])) if size is None else [(start,end)]
        for a,b in ranges:
            off=self.offset(a)
            for ins in self.cs.disasm(self.data[off:off+b-a],a):
                annotations=[]
                for operand in ins.operands:
                    if operand.type==capstone.x86.X86_OP_MEM and operand.mem.base==capstone.x86.X86_REG_RIP:
                        target=ins.address+ins.size+operand.mem.disp
                        annotations.append(f'{target:#x} {self.describe(target)}')
                lines.append(f'{ins.address:#x}: {ins.mnemonic:8} {ins.op_str:48}'+(' ; '+'; '.join(annotations) if annotations else ''))
        return '\n'.join(lines)

    def calls_to(self, *targets):
        targets=set(targets)
        raw=self.text.get_data();base=self.base+self.text.VirtualAddress
        out=[]
        for match in re.finditer(b'\xe8',raw):
            off=match.start()
            if off+5>len(raw): continue
            dest=base+off+5+struct.unpack_from('<i',raw,off+1)[0]
            if dest in targets:
                site=base+off
                out.append({'site':hex(site),'target':hex(dest),'function':hex(self.bounds(site)[0])})
        return out

    def refs_to(self,*targets):
        targets=set(targets)
        raw=self.text.get_data();base=self.base+self.text.VirtualAddress
        out=[]
        for match in re.finditer(rb'[\x40-\x4f][\x8d\x8b\x89\x3b\x39][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]',raw):
            off=match.start()
            if off+7>len(raw): continue
            dest=base+off+7+struct.unpack_from('<i',raw,off+3)[0]
            if dest in targets:
                site=base+off
                out.append({'site':hex(site),'target':hex(dest),'function':hex(self.bounds(site)[0])})
        return out

    def strings(self,text):
        out=[]
        for encoding in ('ascii','utf-16-le'):
            needle=(text+'\0').encode(encoding)
            start=0
            while (start:=self.data.find(needle,start))>=0:
                va=self.va(start)
                out.append({'va':hex(va),'offset':hex(start),'encoding':encoding,
                            'refs':self.refs_to(va)})
                start+=len(needle)
        return out

    def pointers(self,va):
        needle=struct.pack('<Q',va)
        return [self.va(m.start()) for m in re.finditer(re.escape(needle),self.data)]


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['disasm','calls','refs','strings','pointers'])
    parser.add_argument('value')
    parser.add_argument('--size',type=lambda s:int(s,0))
    args=parser.parse_args();img=Image()
    if args.mode=='strings': result=img.strings(args.value)
    else:
        address=int(args.value,0)
        if args.mode=='disasm': print(img.disasm(address,args.size));raise SystemExit
        result={'calls':img.calls_to,'refs':img.refs_to,'pointers':img.pointers}[args.mode](address)
    print(json.dumps(result,indent=2))
