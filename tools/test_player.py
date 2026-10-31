"""Build a real self-contained player, run it and inspect the loaded scene."""
import json
import os
import wave
import copy
import subprocess
from pathlib import Path
out=Path('build/player-test');out.mkdir(parents=True,exist_ok=True)
scene=json.loads(Path('tests/test-project.lunoryx').read_text())
scene['title']='Standalone player smoke test'
# The fixture's character makes the runtime exercise rig and physics paths.
scene['scripts'][0]['code']="set_property(find_object('Sphere'),'PackedScript',123)"
sound_path=out/'source.wav'
with wave.open(str(sound_path),'wb') as f:f.setparams((1,2,22050,0,'NONE','not compressed'));f.writeframes(b'\x00\x00'*2205)
sound=copy.deepcopy(scene['objects'][0]);sound['id']=scene['next_id'];scene['next_id']+=1
sound.update(name='Packed sound',kind=12,parent=0,vertices_f32le='',indices_u32le='',sound=str(sound_path.resolve()),autoplay=True,spatial=False)
scene['objects'].append(sound)
project=out/'scene.lunoryx';project.write_text(json.dumps(scene),encoding='utf-8')
game=out/'game'
for _ in range(2):subprocess.run(['./engine','--build-game',str(project),str(game)],check=True)
assert game.stat().st_size>Path('engine').stat().st_size
assert game.read_bytes()[-8:]==b'LXGAME03'
state=out/'played.lunoryx'
sound_path.unlink()
subprocess.run([str(game.resolve())],env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1',SDL_VIDEODRIVER='offscreen',LUNORYX_DISABLE_AUDIO='1',TEST_TIMEOUT='2',LUNORYX_STATE_PATH=str(state.resolve())),check=True,timeout=30)
loaded=json.loads(state.read_text())
sphere=next(o for o in loaded['objects'] if o['name']=='Sphere')
assert any(p['name']=='PackedScript' and p['number']==123 for p in sphere['properties'])
packed_sound=next(o for o in loaded['objects'] if o['name']=='Packed sound')
assert Path(packed_sound['sound']).is_file() and packed_sound['sound']!=str(sound_path.resolve())
print('PASS: standalone player archive, repeated output overwrite, game startup and embedded Lua execution')
