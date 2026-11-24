from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path.cwd();r=Path(__file__).resolve().parent;engine=r/'engine';project=r/'移动后的完整场景/playable.azureproject';out=r/'published-game'
assert json.loads((r/'save-semantic-verification.json').read_text())['status']=='passed'
assert json.loads((r/'moved-reopen/summary.json').read_text())['status']=='passed'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();tool=engine/'share/AzureRender/tools/build_game.py'
assert sha(tool)==sha(root/'tools/build_game.py'),'Installed builder does not match validated source'
quality=dict(schemaVersion=1,width=1920,height=1080,profile='完整原生场景',acceptance='功能交付候选，正式画质与持续性能按所属门禁验收。')
(project.parent/'QUALITY.json').write_bytes((json.dumps(quality,ensure_ascii=False,indent=2)+'\n').encode())
guide='''# 完整场景操控

双击目录中的项目入口程序启动。
首次运行需要准备完整场景资源。

## 操作

单击游戏窗口获取操控焦点。
WASD 移动，Shift 冲刺，移动鼠标转动相机。
R 重开当前关卡，Esc 释放鼠标和焦点。
同关卡重开包含较长的备用资源准备。

## 画面与范围

默认分辨率为 1920×1080。
项目包含完整物流环境及动态角色。
当前候选用于保存、移动及独立运行的交付验证。
原生画质、时间稳定性及持续性能按正式门禁验收。
'''
(project.parent/'GAME-GUIDE.md').write_bytes(guide.encode())
python=engine/'share/AzureRender/tools/python/python.exe'
command=[str(python),'-X','utf8',str(tool),'--project',str(project),'--install',str(engine),'--output',str(out),'--validation-timeout','1800']
report=dict(status='running',command=command,toolSha256=sha(tool),playerSha256=sha(engine/'bin/AzurePlayer.exe'),projectSha256=sha(project),levelSha256=sha(project.parent/'scene/playable.azurelevel'),qualitySha256=sha(project.parent/'QUALITY.json'),guideSha256=sha(project.parent/'GAME-GUIDE.md'))
env=os.environ.copy();startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;t=time.perf_counter()
(r/'publication.json').write_bytes((json.dumps(report,indent=2,ensure_ascii=False)+'\n').encode())
try:
 with (r/'publication.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=3900)
 report.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0
 manifest=json.loads((out/'game_manifest.json').read_text());assert manifest['format']=='AzureGame v1' and len(manifest['files'])>0
 report.update(status='built-isolation-review-pending',files=len(manifest['files']),entry=manifest['entry'],manifestSha256=sha(out/'game_manifest.json'))
finally:
 if report['status']=='running':report['status']='failed'
 (r/'publication.json').write_bytes((json.dumps(report,indent=2,ensure_ascii=False)+'\n').encode());print(json.dumps(report,indent=2,ensure_ascii=False))
