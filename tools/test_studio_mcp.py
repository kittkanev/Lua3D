"""Verify the MCP interface of the original Studio engine."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path
binary=str(Path(sys.argv[1] if len(sys.argv)>1 else './engine').resolve())
p=subprocess.Popen([binary,'--mcp'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
seq=0
def request(method,params=None):
    global seq
    seq+=1;p.stdin.write(json.dumps({'jsonrpc':'2.0','id':seq,'method':method,'params':params or {}})+'\n');p.stdin.flush()
    response=json.loads(p.stdout.readline());assert response['id']==seq,response
    return response

def call(tool,**args):return request('tools/call',{'name':tool,'arguments':args})['result']
assert request('initialize',{'protocolVersion':'2025-06-18'})['result']['protocolVersion']=='2025-06-18'
assert len(request('tools/list')['result']['tools'])>=11
result=call('add_object',name='Original engine box',type='cube');assert not result['isError']
obj=json.loads(result['content'][0]['text'])['id']
assert not call('set_transform',id=obj,position=[12,3,4])['isError']
assert call('set_transform',id=obj,position=[2,3,4],scale=[0,1,1])['isError']
assert json.loads(call('scene_list')['content'][0]['text'])[0]['position']==[12,3,4]
call('undo_objects');assert json.loads(call('scene_list')['content'][0]['text'])[0]['position']==[0,0,0]
call('redo_objects');assert json.loads(call('scene_list')['content'][0]['text'])[0]['position']==[12,3,4]
assert not call('run_lua',code="local x=find_object('Original engine box'); assert(x==0); set_property(x,'Health',100)")['isError']
assert not call('add_sound',path='sound.wav',name='MCP sound')['isError']
with tempfile.TemporaryDirectory() as tmp:
    project=str(Path(tmp)/'project.lunoryx')
    assert not call('project_save',path=project)['isError']
    data=json.loads(Path(project).read_text());assert data['schema']==2 and data['sounds'][0]['name']=='MCP sound'
    assert not call('remove_object',id=obj)['isError']
    assert not call('project_load',path=project)['isError']
    assert len(json.loads(call('scene_list')['content'][0]['text']))==1
assert request('resources/read',{'uri':'lunoryx://scene'})['result']['contents']
p.stdin.write('[]\n');p.stdin.flush();assert json.loads(p.stdout.readline())['error']['code']==-32600
assert request('ping')['result']=={}
p.stdin.close();p.wait(timeout=10);assert p.returncode==0,p.stderr.read()
print('PASS: original Studio MCP, stable IDs, invalid-transform rollback, undo/redo, sounds and project compatibility')
