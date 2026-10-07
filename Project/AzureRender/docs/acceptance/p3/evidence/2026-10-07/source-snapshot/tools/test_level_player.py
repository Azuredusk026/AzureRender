import json,pathlib,subprocess,sys,tempfile,time
exe=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
 root=pathlib.Path(directory);project=root/'game'
 subprocess.run([str(exe),'--create-project',str(project)],check=True,capture_output=True)
 config=json.loads((project/'project.azureproject').read_text());config['startupScene']='assets:/start.azurelevel';(project/'project.azureproject').write_text(json.dumps(config))
 level={'schemaVersion':1,'id':'json-level','sceneType':'character','resources':[{'id':'mesh','asset':'engine:/assets_public/test_model.gltf'}], 'nodes':[{'id':'hero','resourceId':'mesh','components':{'azure.transform':{'type':'azure.transform','version':1,'data':{'translation':[0,0,0]}}}}]}
 (project/'assets/start.azurelevel').write_text(json.dumps(level));report=root/'runtime.json'
 result=subprocess.run([str(exe),'--project',str(project/'project.azureproject'),'--smoke-frames','4','--fixed-frame-step','--runtime-report',str(report)],cwd=root,capture_output=True,text=True)
 assert result.returncode==0,result.stdout+result.stderr
 assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
 assert 'Allocator after unload: buffers=0 images=0' in result.stdout
 data=json.loads(report.read_text());assert data['scene']=='json-level' and data['nodes'][0]['id']=='hero'
 (root/'captures/gpu_capabilities.json').unlink(missing_ok=True)
 report=root/'reload.json';log=root/'reload.log'
 with log.open('w') as output:
  process=subprocess.Popen([str(exe),'--project',str(project/'project.azureproject'),'--smoke-frames','3000','--fixed-frame-step','--runtime-report',str(report)],cwd=root,stdout=output,stderr=subprocess.STDOUT)
  deadline=time.monotonic()+20
  while not (root/'captures/gpu_capabilities.json').exists():
   assert process.poll() is None,log.read_text()
   assert time.monotonic()<deadline,'Player initialization timed out'
   time.sleep(0.05)
  time.sleep(0.8)
  (project/'assets/start.azurelevel').write_text('{broken')
  time.sleep(0.7)
  level['nodes'][0]['components']['azure.transform']['data']['translation']=[1,0,0]
  (project/'assets/start.azurelevel').write_text(json.dumps(level))
  assert process.wait(timeout=45)==0,log.read_text()
 data=json.loads(report.read_text());assert data['levelRevision']>=2 and data['nodes'][0]['translation'][0]==1,data
 text=log.read_text();assert 'VUID-' not in text and 'Validation Error' not in text and 'Allocator after unload: buffers=0 images=0' in text
 print('Native JSON level, reflected components, failed reload retention and Vulkan hot reload passed')
