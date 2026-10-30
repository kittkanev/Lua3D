"""Create the Windows prerelease bundle from the exact CI artifact."""
import argparse
import hashlib
import re
import shutil
import struct
import zipfile
from pathlib import Path

VERSION = re.search(r'#define LUNORYX_VERSION "([^"]+)"', Path('engine_version.h').read_text()).group(1)
NAME = 'Lunoryx'


def validate_pe(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'Not a PE binary: {path}')
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    if data[offset:offset + 4] != b'PE\0\0' or struct.unpack_from('<H', data, offset + 4)[0] != 0x8664:
        raise ValueError(f'Not a Windows x64 PE binary: {path}')


def package(artifact, output):
    validate_pe(artifact / 'engine.exe')
    binary=(artifact / 'engine.exe').read_bytes()
    pe=struct.unpack_from('<I',binary,0x3c)[0]
    if struct.unpack_from('<H',binary,pe+24+68)[0] != 2:
        raise ValueError('Release executable must use the Windows GUI subsystem')
    if (artifact / 'SDL2.dll').exists():
        raise ValueError('Expected a standalone build without SDL2.dll')
    output.mkdir(parents=True, exist_ok=True)
    stage = output / f'{NAME}-v{VERSION}-windows-x64'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    shutil.copy2(artifact / 'engine.exe', stage / f'{NAME}.exe')
    shutil.copy2(artifact / 'README.md', stage / 'README.md')
    shutil.copy2('docs/README-FIRST.txt', stage / 'README-FIRST.txt')
    for directory in ('examples', 'docs', 'licenses'):
        shutil.copytree(artifact / directory, stage / directory)
    archive = output / f'{stage.name}.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                bundle.write(path, Path(stage.name) / path.relative_to(stage))
    for name in (f'{NAME}.exe',):
        shutil.copy2(stage / name, output / name)
    assets = [archive, output / f'{NAME}.exe']
    (output / 'SHA256SUMS.txt').write_text(''.join(
        f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n' for path in assets), encoding='utf-8')
    print(f'Packaged {NAME} {VERSION}: {archive}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--artifact-dir', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    args = parser.parse_args()
    package(args.artifact_dir, args.output_dir)
