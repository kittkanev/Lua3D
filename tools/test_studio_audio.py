import os
import subprocess
from pathlib import Path
root=Path('build/studio-audio-fixtures');root.mkdir(parents=True,exist_ok=True)
formats={'wav':[], 'w64':[], 'rf64':['-f','wav','-rf64','always'], 'mp3':[], 'flac':[], 'ogg':['-c:a','libvorbis']}
files=[]
for ext,options in formats.items():
    path=root/f'tone.{ext}'
    subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i','sine=frequency=440:duration=0.5',*options,str(path)],check=True);files.append(str(path))
subprocess.run(['make','tests/studio_audio_tests'],check=True)
subprocess.run(['./tests/studio_audio_tests',*files],env=dict(os.environ,LUNORYX_AUDIO_NULL='1'),check=True)
