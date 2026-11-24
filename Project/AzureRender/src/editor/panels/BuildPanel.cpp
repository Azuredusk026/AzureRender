#include "ui/localization/Localization.hpp"
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
    if(!ImGui::Begin(ui::tr("发布项目###build"),workspace_.open("build"))){ImGui::End();return;}
    auto& replace=buildReplace_;
    const auto& receipt=session_->buildResult();
    const bool completed=!session_->building()&&!receipt.message.empty();
    const auto footerHeight=ImGui::GetFrameHeightWithSpacing()*(completed?2.F:1.F);
    if(ImGui::BeginChild("publish-details",{0,-footerHeight})){
        const auto drawInstall=[&]{drawPathInput(ui::tr("引擎安装目录"),installPath_,"install",true);};
        const auto drawOutput=[&]{drawPathInput(ui::tr("发布输出目录"),outputPath_,"output",true);};
        if(ImGui::GetContentRegionAvail().x>=720.F*dpi_ && ImGui::BeginTable("publish-paths",2,ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextColumn();drawInstall();
            ImGui::TableNextColumn();drawOutput();
            ImGui::EndTable();
        }else{
            drawInstall();
            drawOutput();
        }
        ImGui::Checkbox(ui::tr("覆盖已有发布包"), &replace);
        if(session_->building())ui::taskFeedback(session_->buildProgress(),ui::tr("下方显示发布工具报告的实际步骤，任务期间可以查看其他面板。"));
        if(completed){
            ui::resultMessage(receipt.passed?ui::tr("发布完成"):ui::tr("发布失败"),!receipt.passed);
            ImGui::TextWrapped(ui::tr("本次发布目标：%s"),receipt.outputDirectory.u8string().c_str());
            ui::card(ui::tr("Build result"),[&]{ImGui::TextWrapped("%s",receipt.message.c_str());});
        }
    }
    ImGui::EndChild();
    ui::ButtonOptions build;build.variant=ui::ButtonVariant::Primary;build.disabled=!view.isProject()||session_->building()||session_->playing()||installPath_.empty()||outputPath_.empty();
    build.tooltip=ui::tr("需要可编辑项目、Release 引擎安装目录和发布输出目录。");
    if(ui::button(ui::tr("发布 Windows 游戏"),build))session_->startBuild(std::filesystem::u8path(installPath_),std::filesystem::u8path(outputPath_),replace);
    observeWidget("project.build");
    if(completed) {
        if(receipt.passed){
            std::error_code error;
            ui::ButtonOptions reveal;
            reveal.disabled=!std::filesystem::is_directory(receipt.outputDirectory,error);
            if(ui::button(ui::tr("打开发布目录"),reveal))session_->edit("path.reveal",{{"path",receipt.outputDirectory.u8string()}});
            observeWidget("project.build.reveal");
            ui::nextToolbarItem(ui::buttonWidth(ui::tr("查看发布日志")));
        }
        std::error_code error;
        ui::ButtonOptions showLog;
        showLog.disabled=!std::filesystem::is_regular_file(receipt.logFile,error);
        if(ui::button(ui::tr("查看发布日志"),showLog))session_->edit("path.reveal",{{"path",receipt.logFile.u8string()}});
        observeWidget("project.build.log");
    }
    ImGui::End();
}

}
#endif
