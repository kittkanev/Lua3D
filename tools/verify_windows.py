"""Reject console-subsystem builds and non-system runtime DLL dependencies."""
import re
import struct
import subprocess
import sys
from pathlib import Path
path=Path(sys.argv[1])
data=path.read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from('<H',data,pe+24+68)[0]==2, 'Expected Windows GUI subsystem'
imports=subprocess.check_output(['x86_64-w64-mingw32-objdump','-p',str(path)],text=True)
names=re.findall(r'DLL Name:\s*(\S+)',imports)
for name in names:
    assert not any(part in name.lower() for part in ('sdl2','libgcc','libstdc','libwinpthread','libssp')), f'External runtime dependency: {name}'
print('PASS: GUI subsystem; no SDL/GCC/C++/pthread runtime DLLs:', ', '.join(names))
