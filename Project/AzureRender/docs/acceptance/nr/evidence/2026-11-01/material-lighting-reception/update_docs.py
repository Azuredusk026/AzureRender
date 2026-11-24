import json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text())
assert read(run/'regression-pipeline.json')['status']=='passed'
compat=read(run/'compatibility-comparison.json');assert compat['strictCompatibilityPassed'] and compat['headCriteriaPassed']
comparisons=[read(run/f'body-comparison-aa{aa}.json') for aa in [0,3]]
matched=[r for c in comparisons for row in c['cases'] if row['pair']=='world-vs-matched-standalone' for r in row['materials'] if r['coverage']=='measured'];assert len(matched)==36 and all(r['passed'] for r in matched)
minimum=min(r['ssim'] for r in matched);maximum=max(r['medianLuminanceRelativeError'] for r in matched)
related_instances=0;related_frames=0
for cfg in ['release','debug']:
    for p in (run/'related-gpu'/cfg).glob('*-audit.json'):
        audit=read(p);assert audit['status']=='passed'
        for row in audit['players']:
            command=row['command'];related_frames+=int(command[command.index('--capture-frames')+1]);related_instances+=1
def write(name,text):
    p=root/name;data=text.encode()
    if p.exists():
        with p.open('r+b') as f:f.write(data);f.truncate()
    else:p.write_bytes(data)
p='README.md';text=(root/p).read_text();anchor='全身衣料的实际差异、材质区域及辐照响应见[全身对照](docs/acceptance/nr/full-body-native-comparison.md)。\n';assert anchor in text
text=text.replace(anchor,anchor+'\n材质环境来源、主光对象阴影及原生对照见[接收验证](docs/acceptance/nr/material-lighting-reception.md)。\n');write(p,text)
p='docs/plans/azure-engine-plan.md';text=(root/p).read_text();anchor='## 当前执行顺序\n';assert anchor in text
paragraph=f'\n材质[照明接收](../acceptance/nr/material-lighting-reception.md)通过两配置公共回归。\n匹配输入的三十六个不透明区域均通过静态门槛。\n原冻结独立对照的服饰二、五仍有失败项。\n原生服饰变体、完整角色及场景反射继续执行 Q1。\n'
text=text.replace(anchor,anchor+paragraph,1);write(p,text)
p='CHANGELOG.md';text=(root/p).read_text();entry=f'\n## 材质照明接收 2026-11-01\n\n- 画像版本二提供独立的漫反射、反射及主光阴影来源。\n- 普通衣料可消费角色接收组主光及对象图集。\n- 漫反射覆盖与场景探针的辐照适配分别计算。\n- 两配置各通过十七项 CPU 检查及八十四次公共采集。\n- 十四组相关回归完成 {related_frames} 帧及 {related_instances} 次 GPU 进程检查。\n- 八组原生采集完成 282 帧，两条完整操控各完成 540 帧。\n- 匹配照明的三十六个不透明区域通过 0.95 及 10% 门槛。\n- 原冻结独立对照保留裙摆及飘带的实际失败结果。\n- 隐藏环境十二帧及缺省近景逐像素兼容。\n- 公共探针耦合、输入拒绝及夹具失败证据完整保留。\n- 原生服饰、透明合成、完整反射及正式性能继续验收。\n';text=text.replace('# Changelog\n','# Changelog\n'+entry,1);write(p,text)
p='docs/plans/2026-11-01-material-environment-reception.md';text=(root/p).read_text().replace('状态：执行中，归属为 Q1 服饰照明。','状态：公共机制验证完成，归属为 Q1 服饰照明。').replace('- [ ]','- [x]');text+='\n## 检查结果\n\n两配置各通过十七项 CPU 及八十四次公共 GPU 采集。\n八组原生采集完成 282 帧，完整操控各完成 540 帧。\n匹配输入的三十六个不透明区域均通过静态门槛。\n原冻结独立对照保留服饰二、五的失败项。\n\n完整结果与剩余边界见[接收验证](../acceptance/nr/material-lighting-reception.md)。\n原生服饰外观与联合画质继续执行 Q1。\n';write(p,text)
p='docs/acceptance/nr/material-lighting-reception.md';text=(root/p).read_text().replace('状态：公共机制通过，全身及相关回归执行中。','状态：公共机制及受影响回归通过，原生服饰继续验收。').replace('角色近景、隐藏环境和完整操控正在执行回归。','角色近景、隐藏环境和完整操控按下述范围通过回归。')
rows=[];first=comparisons[0]['cases'];original=next(c for c in first if c['frame']==0 and c['pair']=='world-vs-original-standalone');new=next(c for c in first if c['frame']==0 and c['pair']=='world-vs-matched-standalone');labels={0:'身体皮肤',1:'服饰一',2:'服饰二',4:'服饰四',5:'服饰五',8:'脸部'}
for a,b in zip(original['materials'],new['materials']):
    if a['coverage']!='measured':continue
    rows.append(f"| {labels[a['index']]} | {b['ssim']:.6f} | {b['medianLuminanceRelativeError']*100:.2f}% | {a['ssim']:.6f} | {a['medianLuminanceRelativeError']*100:.2f}% |")
addition=f'\n## 原生序列与兼容\n\n四组全身序列各有 64 帧，覆盖两种抗锯齿配置。\n原模型、动作及六份可测不透明区域保持冻结。\n每组按第零、三十一及六十三帧核对。\n匹配输入对照的三十六个区域均通过既有门槛。\n\n匹配对照的最低 SSIM 为 {minimum:.6f}。\n最高中位亮度相对误差为 {maximum*100:.2f}%。\n原冻结独立对照的服饰二、五仍有失败项。\n下表为无抗锯齿首帧的完整结果。\n\n| 区域 | 匹配对照 SSIM | 匹配亮度误差 | 原冻结对照 SSIM | 原冻结亮度误差 |\n| --- | ---: | ---: | ---: | ---: |\n'+'\n'.join(rows)+'\n\n匹配对照证明世界装配对相同输入的消费一致性。\n原冻结对照继续暴露对象图集与级联阴影的形状差异。\n该结果保留为原生服饰及自阴影的接续依据。\n完整原生画面资格和整体验收保持未通过。\n\n![全身对照](evidence/2026-11-01/material-lighting-reception/body-comparison-aa0.png)\n\n'
head=compat['headRegions'];addition+=f"近景五个冻结区域均通过原有静态门槛。\n最低 SSIM 为 {min(r['ssim'] for r in head):.6f}。\n最高中位亮度相对误差为 {max(r['medianLuminanceRelativeError'] for r in head)*100:.2f}%。\n隐藏环境十二帧及缺省近景逐像素兼容。\n\n两配置各通过 540 帧及十三项完整输入回归。\n行走、奔跑、焦点恢复、相机和重启通过行为核验。\n十四组相关回归完成 {related_frames} 帧及 {related_instances} 次 GPU 进程检查。\n成功进程均核对运行诊断及资源卸载归零。\n\n图像、原始失败、构建输入及实际命令见[证据索引](evidence/2026-11-01/material-lighting-reception/manifest.json)。\n正式性能、跨设备及完整原生区域按所属门禁继续执行。\n"
text+=addition;write(p,text)
p='docs/development/artifact-manifest.json';m=read(root/p);entry=dict(path='runs/'+run.name,pathBase='build',purpose='材质环境来源、主光对象阴影及原生全身兼容核验。',retention='相应画质门禁完成并归档后另行清理。',pinMarker='.keep',primaryProject='candidate-project/playable.azureproject',evidence='docs/acceptance/nr/material-lighting-reception.md');assert not any(e['path']==entry['path'] for e in m['preservedRuns']);m['preservedRuns'].append(entry);write(p,json.dumps(m,ensure_ascii=False,indent=2)+'\n')
print('Updated final phase documents; original native appearance remains failed')
