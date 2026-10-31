"""Generate independent codec fixtures with FFmpeg, then decode/mix in C."""
import os
import subprocess
from pathlib import Path
out=Path('build/audio-fixtures');out.mkdir(parents=True,exist_ok=True)
formats={'w64':[], 'rf64':['-f','wav','-rf64','always'], 'wav':[], 'mp3':[], 'flac':[], 'ogg':['-c:a','libvorbis'], 'aiff':['-c:a','pcm_s16be'], 'au':['-c:a','pcm_s16be']}
files=[]
for ext,options in formats.items():
    path=out/f'tone.{ext}'
    subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i','sine=frequency=440:duration=0.5',*options,str(path)],check=True)
    files.append(str(path))
subprocess.run(['make','build/linux/tests/audio_tests'],check=True)
subprocess.run(['./build/linux/tests/audio_tests',*files],env=dict(os.environ,LUNORYX_AUDIO_NULL='1'),check=True)
