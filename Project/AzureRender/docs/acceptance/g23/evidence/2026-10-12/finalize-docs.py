from pathlib import Path
import difflib,json,subprocess
root=Path.cwd();out=root/'build/g23-compressed-audio-2026-10-12';baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(p,s):p.write_text(s,encoding='utf-8')
def shared(name,transform):
 entry=baseline[name];assert (root/name).read_text(encoding='utf-8') in (entry['work'],transform(entry['work'])),name
 save(root/name,transform(entry['work']))
 blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip();subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme='''G23 压缩音频通过非延期自动验收。
Opus/MP3 导出记录有效帧、延迟及填充。
两个项目完成公共制作和双设备移动发布。

证据见[G23 验收](docs/acceptance/g23/2026-10-12.md)。
当前连续执行 P8 完整格式能力收口。
正式性能、长跑及实体复核在 P8 验收。

'''
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 G23 压缩音频与容器交换。','G23 结果按所属阶段来源维护。'))
record='''## G23 压缩音频与容器交换 2026-10-12

新增公共 Opus、AAC/M4A 输入与 Opus/MP3 输出。
运行时使用有预算的预解码与流式数据源。
有效时长、取消、定位与实际混音通过验证。
两个空项目完成制作及双设备隔离移动发布。

证据见[G23 验收](docs/acceptance/g23/2026-10-12.md)。
实体听感、正式性能及长跑在 P8 验收。

'''
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan='''## G23 自动验收结果

两配置各一百七十四项 CPU 回归通过。
两配置各九项受影响 GPU 回归通过。
Opus 与 M4A 空项目完成公共制作及实际混音。
四组设备与项目组合通过隔离移动发布。

证据见[G23 验收](../acceptance/g23/2026-10-12.md)。
当前连续执行 P8 完整格式能力收口。
P5 待项沿用原预算，在 P8 集中验收。

'''
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 G23 压缩音频与容器交换。','G23 结果按所属阶段来源维护。').replace('该路线已获连续执行授权，当前状态为 Planned。','该路线已获连续执行授权，当前状态为 Active。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
 if phase['id']=='G23':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/g23/evidence/2026-10-12/manifest.json')
 if phase['id']=='P8':phase['status']='Active'
d['evidence']['compressedAudio']='docs/acceptance/g23/evidence/2026-10-12/manifest.json'
for capability in d['verifiedCapabilities']:
 if capability['id']=='mp3':capability.update(write=True,exportEvidence='docs/acceptance/g23/evidence/2026-10-12/manifest.json')
for name in ('opus','aac','m4a'):
 d['verifiedCapabilities'].append(dict(id=name,domain='audio',read=True,write=name=='opus',transformsSource=False,settingsVersion=1,generatorVersion=1,evidence='docs/acceptance/g23/evidence/2026-10-12/manifest.json',runtimePath='BoundedFrameReader',decoder='libopusfile' if name=='opus' else 'WindowsMediaFoundation',verifiedChannels=[1,2] if name=='opus' else [2]))
save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='P8';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-next-development.md';s=p.read_text(encoding='utf-8').replace('U15 至 G22 的已实施阶段自动门禁通过。','U15 至 G23 的已实施阶段自动门禁通过。').replace('当前执行 G23 压缩音频与容器交换。','当前执行 P8 完整格式能力收口。').replace('证据见[G22 验收](../acceptance/g22/2026-10-12.md)。','证据见[G23 验收](../acceptance/g23/2026-10-12.md)。',1);save(p,s)
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：G22 自动门禁通过，G23 执行中','状态：G23 自动门禁通过，P8 执行中');a=s.index('## G23：');b=s.index('## P8：',a);section=s[a:b].replace('- [ ]','- [x]').replace('- [x] 验证隔离发布与实体听感。','- [x] 验证隔离发布及自动混音。\n- [ ] 在 P8 集中复核实体听感。');save(p,s[:a]+section+s[b:])
p=root/'docs/plans/2026-10-12-compressed-audio.md';save(p,p.read_text(encoding='utf-8').replace('状态：执行中。','状态：非延期自动门禁通过。').replace('- [ ]','- [x]'))
p=root/'docs/acceptance/g23/2026-10-12.md';save(p,p.read_text(encoding='utf-8').replace('状态：两配置回归与隔离交付验证中。','状态：非延期自动门禁通过。')+'''
## 自动结果

Debug 与 Release 各一百七十四项 CPU 回归通过。
两配置各九项受影响 GPU 回归通过。
两个项目的四组设备组合通过隔离运行。
验证包含实际音源启动、画面及完整资源释放。

证据见[原字节清单](evidence/2026-10-12/manifest.json)。
文风与严格站点构建检查通过。
正式性能、长跑及实体复核保持待验状态。
''')
print('G23 final documentation synchronized with stage baseline')
