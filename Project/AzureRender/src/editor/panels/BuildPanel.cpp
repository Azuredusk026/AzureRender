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
    session_->pollBuild();
    if(!ImGui::Begin("Build Game###build",workspace_.open("build"))){ImGui::End();return;}
    static std::array<char,1024> install{}, output{}, project{};
    static bool replace = false;
    ImGui::InputText("New game directory", project.data(), project.size());
    ImGui::BeginDisabled(session_->building());
    if(ui::button("Create game template")) {
        try { const std::filesystem::path path(project.data()); session_->edit("project.create",{{"path",path.string()},{"name",path.filename().string()}}); }
        catch(const std::exception& error) { session_->log("ERROR: "+std::string(error.what())); }
    }
    ImGui::EndDisabled();
    ImGui::InputText("Release engine directory", install.data(), install.size());
    ImGui::InputText("Game output directory", output.data(), output.size());
    ImGui::Checkbox("Replace existing game package", &replace);
    ImGui::BeginDisabled(!view.isProject() || session_->building() || session_->playing());
    if(ui::button("Build Windows game"))static_cast<void>(session_->startBuild(install.data(),output.data(),replace));
    ImGui::EndDisabled();
    if(session_->building())ImGui::TextUnformatted("Building...");
    else if(!session_->buildResult().message.empty())ImGui::TextWrapped("%s (%.0f ms)",session_->buildResult().message.c_str(),session_->buildResult().milliseconds);
    ImGui::End();
}

}
#endif
