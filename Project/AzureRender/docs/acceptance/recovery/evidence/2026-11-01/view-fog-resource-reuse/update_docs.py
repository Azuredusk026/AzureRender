import json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
def write(path,text):
 p=root/path;data=text.encode()
 with p.open('r+b') as f:f.write(data);f.truncate()
p='README.md';text=(root/p).read_text();anchor='联合可玩候选、普通启动入口及原生响应见[组合验证](docs/acceptance/nr/joint-playable-display.md)。\n';assert anchor in text;text=text.replace(anchor,anchor+'\n含雾关卡的资源身份、重开复用及画面兼容见[复用验证](docs/acceptance/recovery/view-fog-resource-reuse.md)。\n');write(p,text)
p='CHANGELOG.md';text=(root/p).read_text();text=text.replace('# Changelog\n','# Changelog\n\n## 视图雾资源复用 2026-11-01\n\n- 完整的解析后雾绑定及体积摘要共同参与资源身份。\n- 相同绑定重开复用渲染器，并清理时间及运动历史。\n- 两配置各通过十一项相关 CPU 检查。\n- 两配置各通过四种公开工作流及 32 次转场。\n- 完整操控各 540 帧，均与对应冻结基准逐像素一致。\n- 每条完整路线的渲染器 CPU 准备次数由两次降为一次。\n- 固定全景十二帧逐像素一致，十一组成功进程均正常释放。\n- 工具读取采用剩余转场期限，45 秒及 300 秒预算保持有效。\n- 保存资源键、重复准备及工具读取超时的失败证据。\n- 联合画质、全身动态外观及正式性能继续验收。\n',1);write(p,text)
p='docs/plans/azure-engine-plan.md';text=(root/p).read_text();anchor='上述检查保持声明范围，整体验收继续执行。\n';assert anchor in text;text=text.replace(anchor,anchor+'\n含雾关卡的[重开复用](../acceptance/recovery/view-fog-resource-reuse.md)通过两配置回归。\n各 540 帧完整路线及十二帧固定全景逐像素兼容。\n每条完整路线仅执行一次渲染器 CPU 准备。\n加载复用结果按其范围归档，联合画质继续未通过。\n',1);write(p,text)
p='docs/plans/2026-11-01-view-fog-resource-reuse.md';text=(root/p).read_text();text=text.replace('当前相同含雾关卡的重开重复进行资源准备。\n渲染器的启用雾检查直接拒绝复用。\n捕获式雾的部分内联参数缺少资源键覆盖。\n这两项分别由渲染准入及运行时身份持有。','资源键覆盖完整的解析后雾绑定与体积摘要。\n相同绑定的活动实例可进入复用路径。\n不同绑定由宿主按资源身份准备对应实例。\n这两项分别由渲染准入及运行时身份持有。').replace('主辅宿主保持当前资源所有权及关闭顺序。','编辑宿主与 Player 保持当前所有权及关闭顺序。');text=text.replace('- [ ]','- [x]');text+='\n## 检查结果\n\n两配置各通过十一项 CPU 检查及 32 次公开转场。\n各 540 帧完整操控与对应冻结基准逐像素一致。\n固定全景十二帧与显示阶段基准逐像素一致。\n每条完整路线执行一次渲染器 CPU 准备。\n\n完整摘要及剩余条件见[复用验证](../acceptance/recovery/view-fog-resource-reuse.md)。\n联合画质继续保持未通过状态。\n';write(p,text)
p='docs/development/artifact-manifest.json';m=json.loads((root/p).read_text());entry=dict(path='runs/'+run.name,pathBase='build',purpose='视图雾完整资源身份、重开复用及原生画面兼容核验。',retention='相应画质门禁完成并归档后另行清理。',pinMarker='.keep',primaryProject='project/playable.azureproject',evidence='docs/acceptance/recovery/view-fog-resource-reuse.md');assert not any(e['path']==entry['path'] for e in m['preservedRuns']);m['preservedRuns'].append(entry);write(p,json.dumps(m,ensure_ascii=False,indent=2)+'\n')
print('Updated six documents')
