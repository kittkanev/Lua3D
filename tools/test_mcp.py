"""Exercise the shipped stdio server, including rollback and protocol framing."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path
binary = str(Path(sys.argv[1] if len(sys.argv)>1 else './engine').resolve())
p = subprocess.Popen([binary, '--mcp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
seq = 0
def request(method, params=None):
    global seq
    seq += 1
    p.stdin.write(json.dumps({'jsonrpc':'2.0','id':seq,'method':method,'params':params or {}})+'\n')
    p.stdin.flush()
    line = p.stdout.readline()
    assert line, p.stderr.read()
    response = json.loads(line)
    assert response['id'] == seq and response['jsonrpc'] == '2.0', response
    return response

def call(tool_name, **arguments):
    r=request('tools/call', {'name':tool_name,'arguments':arguments})
    assert 'result' in r, r
    return r['result']

assert request('initialize', {'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'integration-test','version':'1'}})['result']['protocolVersion']=='2025-06-18'
p.stdin.write(json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'})+'\n');p.stdin.flush()
assert len(request('tools/list')['result']['tools']) >= 13
assert not call('scene_create', name='Box', kind='cube')['isError']
objects=json.loads(call('scene_list')['content'][0]['text']); obj=objects[0]['id']
assert not call('scene_transform', id=obj, position=[12,3,4])['isError']
assert call('scene_transform', id=obj, scale=[0,1,1])['isError']
assert json.loads(call('scene_list')['content'][0]['text'])[0]['position']==[12,3,4]
assert not call('scene_undo')['isError']
assert json.loads(call('scene_list')['content'][0]['text'])[0]['position']==[0,0,0]
assert not call('scene_redo')['isError']
assert call('script_execute', code='while true do pcall(function() while true do end end) end')['isError']
assert not call('script_execute', code="print('MCP works'); add_object('Sphere','sphere')")['isError']
with tempfile.TemporaryDirectory() as d:
    save=str(Path(d)/'project.lunoryx')
    assert not call('project_save', path=save)['isError']
    assert not call('scene_delete', id=obj)['isError']
    assert not call('project_load', path=save)['isError']
    assert len(json.loads(call('scene_list')['content'][0]['text']))==2
    assert not call('asset_export', path=str(Path(d)/'scene.glb'))['isError']
assert request('resources/read', {'uri':'lunoryx://scene'})['result']['contents']
p.stdin.write('{invalid}\n');p.stdin.flush()
assert json.loads(p.stdout.readline())['error']['code']==-32700
assert request('missing')['error']['code']==-32601
assert request('ping')['result']=={}
p.stdin.close();p.wait(timeout=10)
assert p.returncode==0,p.stderr.read()
print('PASS: MCP initialize, discovery, tools, resources, undo/redo, rollback, script budget, errors and shutdown')
