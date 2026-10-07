#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
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
    if(!ImGui::Begin("Build Game###build",workspace_.open("build"))){ImGui::End();return;}
    static bool replace = false;
    drawPathInput("New game directory",projectPath_,"project",true);
    const auto parent=std::filesystem::u8path(projectPath_).parent_path();
    const bool validProject=!projectPath_.empty()&&!parent.empty()&&std::filesystem::is_directory(parent);
    ui::ButtonOptions create;create.disabled=session_->building()||!validProject;
    create.tooltip="Choose a new directory within an existing parent folder.";
    if(ui::button("Create game template",create)) {
        const auto path=std::filesystem::u8path(projectPath_);
        const auto result=session_->edit("project.create",{{"path",projectPath_},{"name",path.filename().u8string()}});
        if(result)session_->edit("path.remember",{{"purpose","project"},{"path",parent.u8string()},{"directory",true}});
    }observeWidget("project.create");
    ImGui::Separator();
    drawPathInput("Release engine directory",installPath_,"install",true);
    drawPathInput("Game output directory",outputPath_,"output",true);
    ImGui::Checkbox("Replace existing game package", &replace);
    ui::ButtonOptions build;build.disabled=!view.isProject()||session_->building()||session_->playing()||installPath_.empty()||outputPath_.empty();
    build.tooltip="Build requires an idle project, a Release engine installation and an output directory.";
    if(ui::button("Build Windows game",build))session_->startBuild(std::filesystem::u8path(installPath_),std::filesystem::u8path(outputPath_),replace);
    observeWidget("project.build");
    if(session_->building())ImGui::TextUnformatted("Building... You can continue inspecting panels.");
    else if(!session_->buildResult().message.empty())ImGui::TextWrapped("%s (%.0f ms)",session_->buildResult().message.c_str(),session_->buildResult().milliseconds);
    if(!outputPath_.empty())ImGui::TextWrapped("Result directory: %s",outputPath_.c_str());
    ImGui::End();
}

}
#endif
