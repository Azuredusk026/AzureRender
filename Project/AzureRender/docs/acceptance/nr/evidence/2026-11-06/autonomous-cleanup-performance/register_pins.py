from pathlib import Path
import json
OUT=Path(__file__).resolve().parent
E=OUT.parents[2]
p=E/'docs/development/artifact-manifest.json'
m=json.loads(p.read_text(encoding='utf-8'))
known={r['path'] for r in m['preservedRuns']}
purposes={
 'ev0-baseline':'私有来源导入基线及环境证据。',
 'ev6-scene-exchange':'场景交换集成、导出夹具及构建证据。',
 'endfield-native':'原生资产调查、动画候选及来源数据库。',
 'dijiang-lighting-video':'中庭展示工程、照明来源及原速视频。',
 'character-logistics-restoration':'角色与物流还原的工具、候选及来源证据。',
 'endfield-capture':'原生抓帧及官方重放来源证据。',
 'capture-plan-handoff':'原生帧核对、会话收据及工作区快照。',
 'main-material-frame-integration':'主材质帧集成、材质槽与描边绑定证据。',
 'shared-motion-history':'运动向量与时间历史的共享所有权证据。',
 'reflection-motion-depth':'反射、运动及深度边缘的成对控制证据。',
 'reflection-temporal-composition':'反射时间合成的双配置构建及 GPU 证据。',
 'reflection-reprojection':'反射重投影、失败控制及 GPU 核验证据。',
 'main-character-joint-budget':'主角色联合预算、内存诊断与原生捕获。'}
added=[]
for marker in sorted((E/'build/runs').glob('*/.keep')):
    rel=marker.parent.relative_to(E/'build').as_posix()
    if rel in known: continue
    task='-'.join(marker.parent.name.split('-')[2:-1])
    assert task in purposes,task
    row=dict(path=rel,pathBase='build',purpose=purposes[task],retention='逐文件归档覆盖及活动输入核验完成后，按当前会话授权释放。',pinMarker='.keep')
    m['preservedRuns'].append(row)
    added.append(dict(registration=row,markerText=marker.read_text(encoding='utf-8',errors='replace'),reviewedEntries=[f.name for f in marker.parent.iterdir()][:12]))
pins={p.parent.relative_to(E/'build').as_posix() for p in (E/'build/runs').glob('*/.keep')}
registered={r['path'] for r in m['preservedRuns']}
assert pins==registered
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(OUT/'pin-registration.json').write_text(json.dumps(dict(status='passed',pins=len(pins),registered=len(registered),added=added),ensure_ascii=False,indent=2),encoding='utf-8')
print('Pin inventory matches:',len(pins),'added:',len(added))
