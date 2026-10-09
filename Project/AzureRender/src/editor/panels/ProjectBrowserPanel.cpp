#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawProjectBrowserPanel(PanelContext&) {
    if(!ImGui::Begin("Projects###projects",workspace_.open("projects"))){ImGui::End();return;}
    ImGui::TextUnformatted("Create, open and resume a project");
    ImGui::TextWrapped("Each project owns its assets, startup scene and runtime configuration.");
    ImGui::BeginDisabled(session_->playing()||session_->building()||session_->context().importing());
    drawPathInput("Project document",projectOpenPath_,"project.open",false,{"azureproject"});
    if(ui::button("Open project",{ui::ButtonVariant::Primary,false,projectOpenPath_.empty()}))
        session_->edit("project.open",{{"path",projectOpenPath_}});
    observeWidget("project.open");
    ImGui::Separator();ImGui::TextUnformatted("New project");
    ImGui::Combo("Template",&projectTemplate_,"Scene inspection\0Playable game\0");observeWidget("project.template");
    ui::inputText("Name",projectName_);observeWidget("project.name");
    drawPathInput("Parent directory",projectDestination_,"project.create",true);
    ImGui::TextWrapped("The project is created in a new subdirectory named after the project.");
    const bool valid=!projectName_.empty()&&!projectDestination_.empty()&&
        projectName_.find_first_of("/\\:")==std::string::npos&&projectName_!="."&&projectName_!="..";
    ImGui::BeginDisabled(!valid);
    if(ui::button("Create project"))session_->edit("project.create",{{"templateId",projectTemplate_==0?"scene":"game"},
        {"destination",(std::filesystem::u8path(projectDestination_)/std::filesystem::u8path(projectName_)).u8string()},{"name",projectName_}});
    observeWidget("project.create");ImGui::EndDisabled();
    ImGui::Separator();ImGui::TextUnformatted("Recent projects");
    for(const auto& project:session_->projects().recent()){
        const auto path=project.at("path").get<std::string>();ImGui::PushID(path.c_str());
        const bool available=project.at("available").get<bool>();
        ImGui::BeginDisabled(!available);
        if(ImGui::Selectable(project.at("name").get<std::string>().c_str()))session_->edit("project.open",{{"path",path}});
        observeWidget("project.recent."+project.at("id").get<std::string>());ImGui::EndDisabled();
        ImGui::TextWrapped("%s",path.c_str());
        if(!available){
            ImGui::TextDisabled("Project unavailable. Locate its document to recover it.");
            if(ui::button("Locate document"))projectOpenPath_=path;
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();ImGui::End();
}
}
#endif
