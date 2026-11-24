import json,pathlib,subprocess,sys,tempfile
exe=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
 root=pathlib.Path(directory);project=root/'game';subprocess.run([str(exe),'--create-project',str(project)],check=True,capture_output=True)
 config=json.loads((project/'project.azureproject').read_text());config['startupScene']='assets:/physics.azurelevel';(project/'project.azureproject').write_text(json.dumps(config))
 def component(t,data):return {'type':t,'version':1,'data':data}
 level={'schemaVersion':1,'id':'physics','sceneType':'character','resources':[{'id':'mesh','asset':'engine:/assets_public/test_model.gltf'}],'nodes':[
 {'id':'floor','components':{'azure.transform':component('azure.transform',{'translation':[0,-1,0]}),'azure.rigid-body':component('azure.rigid-body',{'halfExtent':[10,1,10]})}},
 {'id':'hero','resourceId':'mesh','components':{'azure.transform':component('azure.transform',{'translation':[0,3,0]}),'azure.character':component('azure.character',{})}}]}
 (project/'assets/physics.azurelevel').write_text(json.dumps(level));report=root/'report.json'
 result=subprocess.run([str(exe),'--project',str(project/'project.azureproject'),'--smoke-frames','180','--fixed-frame-step','--runtime-report',str(report)],cwd=root,capture_output=True,text=True)
 assert result.returncode==0,result.stdout+result.stderr
 data=json.loads(report.read_text());hero=next(node for node in data['nodes'] if node['id']=='hero')
 assert data['fixedSteps']==179 and 0.8<hero['translation'][1]<1.2,data
 assert 'VUID-' not in result.stderr and 'Allocator after unload: buffers=0 images=0' in result.stdout
 print('Vulkan Player fixed-step character fall and ground collision passed')
