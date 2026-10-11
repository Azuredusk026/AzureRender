from pathlib import Path
import json,subprocess
root=Path.cwd();out=root/'build/g20-dependency-evaluation-2026-10-12'
for config in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 168' in (out/(config+'-cpu.log')).read_text(encoding='utf-8')
    assert '100% tests passed, 0 tests failed out of 4' in (out/(config+'-gpu.log')).read_text(encoding='utf-8')
assert json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'))['passed']
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(path,text):
    temporary=path.with_suffix(path.suffix+'.g20tmp');temporary.write_text(text,encoding='utf-8');temporary.replace(path)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8')==entry['work'],name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme='''G20 音频解码与生命周期通过自动验收。
四种输入共用导入、重导入及播放接口。
两配置各一百六十八项 CPU 回归通过。
四格式制作项目通过双设备移动发布检查。

指南见[音频导入](docs/runtime/audio-format-import.md)。
证据见[G20 验收](docs/acceptance/g20/2026-10-12.md)。
当前连续执行 G21 通用导出阶段。

'''
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 G20 音频格式阶段。','图像结果按所属阶段来源维护。'))
record='''## G20 音频解码与生命周期 2026-10-12

新增 WAV、MP3、FLAC 与 Ogg Vorbis 公共输入。
短音效预解码，长音频采用实际流式数据源。
修复 Unicode 路径与流式 Vorbis 长度查询。
公共保存、重导入、混音及场景释放通过。

阶段证据见[G20 验收](docs/acceptance/g20/2026-10-12.md)。
正式性能、长跑和实体听感在 P8 验收。

'''
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan='''## G20 自动验收结果

G20 四格式音频解码与生命周期自动门禁通过。
两配置各一百六十八项 CPU 回归通过。
两配置各四项受影响 GPU 回归通过。
四格式制作项目完成双设备移动发布检查。

证据见[G20 验收](../acceptance/g20/2026-10-12.md)。
当前连续执行 G21 通用导出阶段。
正式性能、长跑及集中人工复核在 P8 验收。

'''
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 G20 音频格式阶段。','G19 结果按所属阶段来源维护。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
    if phase['id']=='G20':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/g20/evidence/2026-10-12/manifest.json')
    if phase['id']=='G21':phase['status']='Active'
for format_id in ('wav','mp3','flac','vorbis'):
    d['verifiedCapabilities'].append({'id':format_id,'domain':'audio','read':True,'write':False,'transformsSource':False,'settingsVersion':1,'generatorVersion':1,'evidence':'docs/acceptance/g20/evidence/2026-10-12/manifest.json'})
save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='G21';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：G19 自动门禁通过，G20 执行中','状态：G20 自动门禁通过，G21 执行中')
a=s.index('## G20：');b=s.index('## G21：',a);section=s[a:b].replace('- [ ]','- [x]').replace('- [x] 记录操作者听感与输出设备。','- [ ] 在 P8 记录操作者听感与输出设备。');s=s[:a]+section+s[b:]
s=s.replace('| 音频登记 | WAV |','| 音频登记 | WAV、MP3、FLAC、Ogg Vorbis |').replace('| 音频运行 | miniaudio 文件加载、播放与离线混音 |','| 音频运行 | 统一探测、预解码或流式播放及离线混音 |');save(p,s)
p=root/'docs/plans/2026-10-11-next-development.md';s=p.read_text(encoding='utf-8').replace('当前执行 G20 音频解码与资源生命周期。','当前执行 G21 通用导出与往返验证。').replace('U15 至 G19 的已实施阶段自动门禁通过。','U15 至 G20 的已实施阶段自动门禁通过。').replace('证据见[G19 验收](../acceptance/g19/2026-10-12.md)。','证据见[G20 验收](../acceptance/g20/2026-10-12.md)。',1).replace('G17、G18 与 G19 的自动门禁通过。','G17 至 G20 的自动门禁通过。').replace('G20 至 P8 按本表连续推进。','G21 至 P8 按本表连续推进。');save(p,s)
p=root/'docs/runtime/asset-formats.md';save(p,p.read_text(encoding='utf-8')+'''
## 音频输入

WAV、MP3、FLAC 与 Ogg Vorbis 通过公共格式注册。
导入与重导入验证全部样本，并保留资产标识。
短音效预解码，长音频采用实际流式数据源。
契约见[音频导入](audio-format-import.md)。
''')
acceptance='''# G20 音频解码与生命周期验收

> 日期：2026-10-12。
> 状态：非延期自动门禁通过，最终验收在 P8 执行。

## 公共实现

四种输入通过格式注册进入共享传输服务。
RenderCore 拥有解码、转换及完整样本验证。
Runtime 拥有设备、混音及声音生命周期。
编辑器与项目使用稳定资产标识引用音频。

契约见[音频导入](../../runtime/audio-format-import.md)。
依赖版本与许可见[源码清单](../../licenses/audio-codecs-source.json)。
夹具生成源码采用 `CC0-1.0`。
真实听感与输出设备在 P8 记录。

## 自动验证

两配置各一百六十八项 CPU 回归通过。
两配置各四项受影响 GPU 回归通过。
范围含呈现、可选运行、热重载及完整游戏串联。
模块边界检查覆盖十七个目标，违规数为零。

独立正弦、脉冲和静音验证实际样本。
无损 FLAC 样本与 WAV 的原始样本一致。
有损输入通过幅度、频谱及时长断言。
转换验证采样率、双声道及时间保持。

循环、暂停、恢复与重复释放通过。
三十一秒 WAV 和 Vorbis 使用实际流式数据源。
长音频验证支持中途取消。
中文、表情字符及大写扩展名读取通过。

公共工作流验证导入、重导入和保存重开。
资源包在来源移除后仍可解码全部四种输入。
实体销毁与场景替换释放音源所有权。
编辑组件重开后进入真实离线混音。

移动安装的文件清单逐字节验证。
两个宿主在隔离路径下检查资源与启动。
四格式项目隐藏来源和引擎目录后独立运行。
NVIDIA 与 Intel 的音源启动数均为四。

## 诊断与边界

长夹具使用每块 4096 帧的 Vorbis 编码。
单块全部样本的栈溢出证据按原字节保留。
流式后端长度不可查询时采用已探测时长。
运行时的格式及流式标志仍来自实际数据源。

截断、错误容器、超预算和取消均有失败断言。
实际流式输入验证离线样本输出及释放。
设备听感、正式性能及长跑由 P8 集中验收。
证据见[原字节清单](evidence/2026-10-12/manifest.json)。
'''
p=root/'docs/acceptance/g20/2026-10-12.md';p.parent.mkdir(parents=True,exist_ok=True);save(p,acceptance)
print('Synchronized G20 documents and advanced the automatic phase to G21')
