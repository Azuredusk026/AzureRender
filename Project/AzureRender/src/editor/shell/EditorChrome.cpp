#include "EditorChrome.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
#include "editor/ui/ObjectCreationMenu.hpp"
#include "ui/AppChrome.hpp"
#include "ui/BrandMark.hpp"
#include <algorithm>
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
namespace {
constexpr ImGuiWindowFlags barFlags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize
    |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings
    |ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
}
bool EditorChrome::enabled(EditorSession& s,EditorCommand command) {
    switch(command) {
    case EditorCommand::Play:case EditorCommand::Simulate:return !s.playing()&&!s.building();
    case EditorCommand::Pause:return s.runtime()&&s.runtime()->state()==RuntimeLifecycle::State::Running;
    case EditorCommand::Resume:case EditorCommand::Step:return s.runtime()&&s.runtime()->state()==RuntimeLifecycle::State::Paused;
    case EditorCommand::Stop:return s.playing();
    case EditorCommand::Undo:return !s.playing()&&!s.building()&&s.context().canUndo();
    case EditorCommand::Redo:return !s.playing()&&!s.building()&&s.context().canRedo();
    case EditorCommand::Save:case EditorCommand::Reload:return !s.playing()&&!s.building();
    default:return true;
    }
}
void EditorChrome::draw(EditorSession& s,EditorWorkspace& workspace,float dpi,const Observer& observe) {
    const auto* vp=ImGui::GetMainViewport();const auto layout=EditorWorkspace::layout(vp->Size.x,vp->Size.y,dpi,s.settings().get("editor.compact").get<bool>());
    ImGui::SetNextWindowPos(vp->Pos);ImGui::SetNextWindowSize({vp->Size.x,layout.menu});
    ImGui::Begin("Menu###editor-menu",nullptr,barFlags|ImGuiWindowFlags_MenuBar);
    if(ImGui::BeginMenuBar()) {
        ui::brandMark(ImGui::GetFontSize());ImGui::SameLine();
        auto item=[&](const char* label,const char* shortcut,EditorCommand command){
            if(ImGui::MenuItem(label,shortcut,false,enabled(s,command)))static_cast<void>(s.execute(command));
            observe(std::string("menu.")+label);
        };
        if(ImGui::BeginMenu("文件")){if(ImGui::MenuItem("项目中心…"))workspace.setVisible("projects",true);observe("menu.Projects");item("Save","Ctrl+S",EditorCommand::Save);item("Reload",nullptr,EditorCommand::Reload);ImGui::EndMenu();}observe("menu.File");
        if(ImGui::BeginMenu("编辑")){item("Undo","Ctrl+Z",EditorCommand::Undo);item("Redo","Ctrl+Y",EditorCommand::Redo);item("Reload Assets",nullptr,EditorCommand::ReloadAssets);
            ImGui::Separator();
            if(ImGui::MenuItem("项目设置…")){workspace.settingsCategory=1;workspace.setVisible("settings",true);ImGui::SetWindowFocus("设置###settings");}observe("menu.project-settings");
            if(ImGui::MenuItem("个人偏好…")){workspace.settingsCategory=0;workspace.setVisible("settings",true);ImGui::SetWindowFocus("设置###settings");}observe("menu.preferences");
            ImGui::EndMenu();}observe("menu.Edit");
        if(ImGui::BeginMenu("窗口")){
            for(auto& panel:workspace.panels()) {
                std::string title=panel.title.substr(0,panel.title.find("###"));
                ImGui::MenuItem(title.c_str(),nullptr,&panel.visible);observe("panel."+panel.id);observe("tool."+panel.id);
            }
            ImGui::Separator();
            for(const auto* preset:{"authoring","debugging"}){if(ImGui::MenuItem(preset))s.edit("workspace.preset",{{"id",preset}});observe(std::string("layout.")+preset);}
            item("Reset Layout",nullptr,EditorCommand::ResetLayout);ImGui::EndMenu();
        }observe("menu.View");
        if(ImGui::BeginMenu("物体")) {
            ImGui::BeginDisabled(s.playing()||s.building());
            const auto parent=s.context().selectedNode()?s.context().selectedNode()->id:std::string();
            ui::objectCreationMenu(s.nodePresets().describe(),[&](const auto& preset){s.edit("node.create-preset",{{"preset",preset},{"parent",parent}});},observe);
            ImGui::EndDisabled();ImGui::EndMenu();
        }observe("menu.GameObject");
        if(ImGui::BeginMenu("帮助")){ImGui::TextUnformatted("Editor tutorial: docs/tutorials/editor-first-game.md");ImGui::TextUnformatted("W/E/R: transform | F: frame selection | Ctrl+S: save");ImGui::EndMenu();}observe("menu.Help");
        ImGui::EndMenuBar();
    }ImGui::End();
    ImGui::SetNextWindowPos({vp->Pos.x,vp->Pos.y+layout.menu});ImGui::SetNextWindowSize({vp->Size.x,layout.toolbar});
    ImGui::Begin("Toolbar###editor-toolbar",nullptr,barFlags);
    auto button=[&](const char* title,const char* id,EditorCommand command){
        ui::ButtonOptions options;options.variant=ui::ButtonVariant::Toolbar;options.disabled=!enabled(s,command);options.tooltip=options.disabled?"当前运行或构建状态下不可用":title;
        if(ui::button(title,options))static_cast<void>(s.execute(command));observe(id);
        if(!enabled(s,command)&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("当前运行或构建状态下不可用");
        ImGui::SameLine();
    };
    button("保存","save",EditorCommand::Save);
    auto& context=s.context();
    const bool paused=s.runtime()&&s.runtime()->state()==RuntimeLifecycle::State::Paused;
    const auto gap=ImGui::GetStyle().ItemSpacing.x;
    const auto runWidth=ui::buttonWidth("运行")+ui::buttonWidth(paused?"继续":"暂停")+ui::buttonWidth("停止")+3*gap;
    const auto fullWidth=runWidth+ui::buttonWidth("模拟")+ui::buttonWidth("单步")+2*gap;
    const auto rightWidth=ui::buttonWidth("发布项目")+ui::buttonWidth("更多")+3*gap;
    const bool full=ImGui::GetContentRegionAvail().x>=fullWidth+rightWidth;
    const auto groupWidth=full?fullWidth:runWidth;
    const auto position=ui::toolbarGroupStart(ImGui::GetWindowWidth(),ImGui::GetCursorPosX(),groupWidth,rightWidth);
    ImGui::SameLine(position);
    button("运行","play",EditorCommand::Play);
    if(full)button("模拟","simulate",EditorCommand::Simulate);
    button(paused?"继续":"暂停",paused?"resume":"pause",paused?EditorCommand::Resume:EditorCommand::Pause);
    if(full)button("单步","step",EditorCommand::Step);
    button("停止","stop",EditorCommand::Stop);
    if(ui::button("发布项目")){workspace.setVisible("build",true);ImGui::SetWindowFocus("发布项目###build");}observe("build");
    if(!full){ui::nextToolbarItem(ui::buttonWidth("更多"));if(ui::button("更多"))ImGui::OpenPopup("toolbar-more");observe("toolbar.more");
        if(ImGui::BeginPopup("toolbar-more")){
            if(ImGui::MenuItem("模拟",nullptr,false,enabled(s,EditorCommand::Simulate)))static_cast<void>(s.execute(EditorCommand::Simulate));observe("simulate");
            if(ImGui::MenuItem("单步",nullptr,false,enabled(s,EditorCommand::Step)))static_cast<void>(s.execute(EditorCommand::Step));observe("step");
            ImGui::EndPopup();
        }
    }
    static_cast<void>(context);
    ImGui::End();
}
void EditorChrome::status(EditorSession& s,float dpi) {
    const auto* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->Pos.x,vp->Pos.y+vp->Size.y-24*dpi});ImGui::SetNextWindowSize({vp->Size.x,24*dpi});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{8*dpi,2*dpi});
    ImGui::Begin("Status###editor-status",nullptr,barFlags);
    const auto& c=s.context();
    ImGui::Text("%s | %s%s",c.isProject()?c.project().name.c_str():"Scene",c.scene().sceneId.c_str(),c.dirty()?" * 未保存":"");
    if(s.building()){ImGui::SameLine();ImGui::TextUnformatted("正在发布项目…");}
    else if(c.importing()){ImGui::SameLine();ImGui::Text("Import %.0f%%",c.importProgress()*100);}
    for(const auto& task:s.tasks().report())if(task.at("state")=="running"){ImGui::SameLine();ImGui::Text("%s %.0f%%",task.at("source").get<std::string>().c_str(),task.at("progress").get<float>()*100);break;}
    if(!s.lastError().empty()){ImGui::SameLine();ui::resultMessage(s.lastError(),true);}
    const char* help="操作提示";
    const float helpWidth=ImGui::CalcTextSize(help).x;
    const float contentRight=ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x;
    if(contentRight+helpWidth+24*dpi<ImGui::GetWindowWidth()){
        ImGui::SameLine(ImGui::GetWindowWidth()-helpWidth-16*dpi);ImGui::TextDisabled("%s",help);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("右键拖动：飞行视角\nWASD：移动，E/Q：升降\nShift：加速，右键加滚轮：飞行速度\nAlt 加左键：环绕，中键：平移，滚轮：缩放\n%s / %s / %s：移动 / 旋转 / 缩放\nF：聚焦选择，Ctrl+S：保存，Ctrl+P：运行或停止",
            s.settings().get("editor.shortcuts.move").get<std::string>().c_str(),
            s.settings().get("editor.shortcuts.rotate").get<std::string>().c_str(),
            s.settings().get("editor.shortcuts.scale").get<std::string>().c_str());
    }
    ImGui::End();ImGui::PopStyleVar();
}
}
#else
namespace azurerender {
void EditorChrome::draw(EditorSession&,EditorWorkspace&,float,const Observer&){}
void EditorChrome::status(EditorSession&,float){}
}
#endif
