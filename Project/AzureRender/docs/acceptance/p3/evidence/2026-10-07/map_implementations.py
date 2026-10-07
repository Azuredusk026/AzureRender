"""Connect every adopted design to actual public interfaces and phase evidence."""
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
path=root/'docs/plans/engine-evolution-manifest.json';manifest=json.loads(path.read_text(encoding='utf-8'))
mapping={
'A1':(['ModuleAssembly','SystemRegistry'],['src/runtime/ModuleAssembly.hpp','src/runtime/SystemRegistry.hpp'],
       ['AzureEngine.ModuleAssembly','AzureEngine.SystemComposition','AzureEngine.ModuleBoundaries'],
       '进程内模块装配与单向依赖采用。模块声明所有权、线程和关闭顺序。'),
'A2':(['ComponentRegistry','Property'],['src/runtime/ComponentRegistry.hpp','src/reflection/Registry.hpp'],
       ['AzureEngine.ComponentRegistry','AzureEngine.MetadataGeneration','AzureEngine.RegisteredComponentHost'],
       '稳定类型与属性元数据采用。编辑、工具和脚本共用校验规则。'),
'A3':(['EditService','EditTransaction'],['src/editor/commands/EditService.hpp','src/editor/commands/EditTransaction.hpp'],
       ['AzureEditor.EditService','AzureEditor.EditTransaction','AzureEngine.EditorHost'],
       '统一编辑操作与文档事务采用。批量恢复与版本拒绝由公共入口执行。'),
'A4':(['ObservationRegistry'],['src/runtime/ObservationRegistry.hpp'],
       ['AzureEngine.Observation','AzureEditor.ObservationHost','AzureTools.ValidationProtocol'],
       '注册查询与条件等待采用。生产状态在宿主线程读取。'),
'A5':(['GeneratorRegistry','GenerationManifest'],['src/assets/GeneratorRegistry.hpp','src/assets/GenerationManifest.hpp'],
       ['AzureAssets.GenerationManifest','AzureAssets.ProceduralHost'],
       '文本生成与来源清单采用。输入、版本、输出和许可形成可核对记录。'),
'A6':(['ProposalController','IProposalAdapter','EditTransaction'],
       ['src/editor/ai/ProposalController.hpp','src/editor/ai/ProposalContracts.hpp','src/editor/commands/EditTransaction.hpp'],
       ['AzureEditor.ProposalController','AzureEditor.ProposalAdapter','AzureEditor.ProposalHost','AzureEditor.EditTransaction'],
       '有界提案与领域校验采用。候选关联基础版本，经授权操作和事务应用。'),
'B1':(['ThemeTokens','UiMetrics'],['src/editor/ui/ThemeTokens.hpp','src/editor/ui/UiMetrics.hpp'],
       ['AzureEditor.UiFoundation','AzureEditor.WorkspaceGpu'],
       '语义样式、尺度与公共控件采用。字体和 DPI 验证来自生产工作区。'),
'B2':(['PanelContext','SelectionService'],['src/editor/PanelContext.hpp','src/editor/SelectionService.hpp'],
       ['AzureEditor.PanelContext','AzureEditor.WorkspaceGpu'],
       '面板上下文与选择服务采用。文档修改消费同一编辑服务。'),
'B3':(['SettingRegistry','SettingSource'],['src/foundation/SettingRegistry.hpp'],
       ['AzureEngine.SettingRegistry','AzureEngine.SettingsOverlay','AzureEditor.SettingsHost'],
       '类型化设置与来源优先级采用。持久化偏好和帧边界应用各有归属。'),
'B4':(['IModelTransport','ModelClient','IProposalAdapter'],['src/ai/ModelClient.hpp','src/editor/ai/ProposalContracts.hpp'],
       ['AzureAI.ModelProtocol','AzureAI.Bridge','AzureEditor.ProposalController'],
       '独立模型进程与版本化协议采用。可选服务使用固定响应完成自动验收。'),
'B5':(['runtime_command','control'],['tools/azure.py'],
       ['AzureTools.Cli','AzureTools.ValidationProtocol'],
       '薄命令入口采用。构建、查询、编辑与验证经生产命令执行。'),
'B6':(['IShaderHotReloader','ShaderHotReloader'],['src/render/IShaderHotReloader.hpp','src/devtools/ShaderHotReloader.hpp'],
       ['AzureRender.HotReload','AzurePlatform.ProcessRunner','AzureEditor.DevtoolsHost'],
       '重载契约与开发期模块边界采用。成功候选在安全边界替换资源。'),
'B7':(['RenderViewService','AssetThumbnailService'],['src/render/RenderViewService.hpp','src/editor/preview/AssetThumbnailService.hpp'],
       ['AzureRender.RenderView','AzureEditor.Thumbnail','AzureRender.PreviewHost'],
       '独立视图和有界缩略图缓存采用。视口、预览与缓存具有独立生命周期。'),
'B8':(['SystemRegistry','InteractionPolicy'],['src/runtime/SystemRegistry.hpp','src/runtime/InteractionRuntime.hpp'],
       ['AzureEngine.SystemComposition','AzureEngine.SceneInspectorHost','AzureEngine.PlayablePlayer'],
       '共享机制与应用规则分层采用。探索与检视项目以配置组合各自行为。')}
c_files={
'C1':['src/assets/generators/ProceduralContracts.hpp','src/assets/generators/RigTextContracts.hpp'],
'C2':['tools/compile_shader_module.py','src/render/ShaderSharedTypes.hpp','shaders/modules/interfaces/BlockSamples.slang'],
'C3':['src/render/ResourceAccessProfile.hpp','src/render/ResourceIndexTable.hpp',
      'src/render/VisibilityPrototype.hpp','src/render/SkinningBatch.hpp','src/scene/RenderBatching.hpp'],
'C4':['src/runtime/IScriptRuntime.hpp','src/runtime/ScriptRuntimeRegistry.hpp','src/scripting/ScriptBindingHost.hpp',
      'src/scripting/ScriptInterop.hpp','src/scripting/ScriptHostSession.hpp','src/scripting/dotnet/ManagedScriptRuntime.hpp']}
limits={
'A1':['Windows 进程内装配，通用动态插件属于专项范围。'],
'A2':['新增类型须注册版本和组件能力。'],
'A3':['文档事务覆盖受控编辑操作，应用外部效果有独立恢复规则。'],
'A4':['本机控制通道使用回环地址与会话令牌。'],
'A5':['生成器明确支持语义、输入规模和期限。'],
'A6':['候选通过领域校验，实体键鼠复核按实际执行报告。'],
'B1':['Windows 编辑器与 Dear ImGui 范围。'],
'B2':['面板消费受限文档视图和规定操作。'],
'B3':['启动专用设置需重新启动生效。'],
'B4':['实际云模型输出质量未作为自动验收依据。'],
'B5':['本机命令工具，模型凭据归接入服务。'],
'B6':['开发期着色重载；通用代码热替换属于专项范围。'],
'B7':['容量、色彩传递与完成后释放遵守视图契约。'],
'B8':['具体任务规则归应用脚本，核心调度通过配置装配。'],
'C1':['受限几何与刚体动画，完整 SCAD 语义属于专项范围。'],
'C2':['可选两策略 Bloom 原型，默认 GLSL 渲染。'],
'C3':['资源契约采用，可见性原型保留；CPU 筛选参与正式提交。'],
'C4':['默认 Lua；托管内容受信任；CoreCLR 依赖外部运行库。']}
phases={p['id']:p for p in manifest['phases']}
history={}
for line in subprocess.check_output(['git','log','--format=%H%x00%s'],cwd=root).decode('utf-8').splitlines():
    commit,title=line.split('\x00',1);history.setdefault(title,commit)
for phase in phases.values():
    if phase['id']!='P3':phase['verification']['commit']=history[phase['verification']['commitTitle']]
for item in manifest['items']:
    ident=item['id']
    if ident in mapping:
        symbols,files,tests,scope=mapping[ident]
        item['implementation']={'interfaces':symbols,'interfaceFiles':files,'tests':tests,'scope':scope,
            'verification':phases[item['phases'][-1]]['verification'].copy()}
    else:item['implementation']['interfaceFiles']=c_files[ident]
    implementation=item['implementation']
    implementation['verification']['commit']=phases[item['phases'][-1]]['verification']['commit']
    implementation['riskResolution']={'tests':implementation['tests'],'limits':limits[ident]}
path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('All 18 implementation mappings updated')
