"""Regenerate the C font and license resources; no runtime files are required."""
import json
from pathlib import Path
font=Path('assets/Roboto-Regular.ttf').read_bytes()
with Path('native/font_data.h').open('w') as f:
    f.write('/* Roboto Regular, Apache-2.0. Embedded to keep the executable standalone. */\nstatic const unsigned char lx_font_data[] = {\n')
    for i in range(0,len(font),24):f.write(','.join(str(v) for v in font[i:i+24])+',\n')
    f.write('};\n')
names=['SDL2.txt','Lua.txt','Nuklear.txt','cgltf.txt','cJSON.txt','miniaudio.txt','stb.txt','Roboto-Apache-2.0.txt','GCC-GPL-3.txt','GCC-Runtime-Exception.txt']
with Path('native/licenses.h').open('w') as f:
    f.write('/* Generated from docs/licenses by tools/embed_assets.py. */\nstatic const struct { const char *name,*text; } lx_licenses[] = {\n')
    for name in names:
        f.write('{'+json.dumps(name)+',\n')
        f.write('\n'.join(json.dumps(line+'\n') for line in Path('docs/licenses',name).read_text().splitlines()))
        f.write('},\n')
    f.write('};\n')
