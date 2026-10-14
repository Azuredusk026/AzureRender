#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height) {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawBuildPanel(PanelContext& panelContext) {
    const auto& view=panelContext.view();
    if(!ImGui::Begin("发布项目###build",workspace_.open("build"))){ImGui::End();return;}
    auto& replace=buildReplace_;
    ui::sectionHeading("发布 Windows 游戏","生成可移动的独立游戏目录，包含程序、运行依赖、资源与配置。");
    drawPathInput("新项目目录",projectPath_,"project",true);
    const auto parent=std::filesystem::u8path(projectPath_).parent_path();
    const bool validProject=!projectPath_.empty()&&!parent.empty()&&std::filesystem::is_directory(parent);
    ui::ButtonOptions create;create.disabled=session_->building()||!validProject;
    create.tooltip="选择已有父目录下的新项目路径。";
    if(ui::button("创建可玩项目",create)) {
        const auto path=std::filesystem::u8path(projectPath_);
        const auto result=session_->edit("project.create",{{"path",projectPath_},{"name",path.filename().u8string()}});
        if(result)session_->edit("path.remember",{{"purpose","project"},{"path",parent.u8string()},{"directory",true}});
    }observeWidget("project.create");
    ImGui::Separator();
    drawPathInput("引擎安装目录",installPath_,"install",true);
    drawPathInput("发布输出目录",outputPath_,"output",true);
    ImGui::Checkbox("覆盖已有发布包", &replace);
    ui::ButtonOptions build;build.variant=ui::ButtonVariant::Primary;build.disabled=!view.isProject()||session_->building()||session_->playing()||installPath_.empty()||outputPath_.empty();
    build.tooltip="需要可编辑项目、Release 引擎安装目录和发布输出目录。";
    if(ui::button("发布 Windows 游戏",build))session_->startBuild(std::filesystem::u8path(installPath_),std::filesystem::u8path(outputPath_),replace);
    observeWidget("project.build");
    if(session_->building())ui::taskFeedback(session_->buildProgress(),"下方显示发布工具报告的实际步骤，任务期间可以查看其他面板。");
    else if(!session_->buildResult().message.empty()) {
        ui::resultMessage(session_->buildResult().passed?"发布完成":"发布失败",!session_->buildResult().passed);
        ui::card("Build result",[&]{ImGui::TextWrapped("%s",session_->buildResult().message.c_str());});
    }
    if(!outputPath_.empty())ImGui::TextWrapped("输出目录：%s",outputPath_.c_str());
    ui::ButtonOptions reveal;reveal.disabled=outputPath_.empty()||!std::filesystem::is_directory(std::filesystem::u8path(outputPath_));
    if(ui::button("打开发布目录",reveal))session_->edit("path.reveal",{{"path",outputPath_}});observeWidget("project.build.reveal");
    if(view.isProject()) {
        ImGui::SameLine();const auto log=session_->context().project().file.parent_path()/".azure/game-build.log";
        ui::ButtonOptions showLog;showLog.disabled=!std::filesystem::is_regular_file(log);
        if(ui::button("查看发布日志",showLog))session_->edit("path.reveal",{{"path",log.u8string()}});observeWidget("project.build.log");
    }
    ImGui::End();
}

}
#endif
