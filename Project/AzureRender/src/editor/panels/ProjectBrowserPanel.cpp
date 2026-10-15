#include "ui/localization/Localization.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawProjectBrowserPanel(PanelContext&) {
    if(!ImGui::Begin(ui::tr("项目中心###projects"),workspace_.open("projects"))){ImGui::End();return;}
    ui::sectionHeading(ui::tr("Projects"),ui::tr("Create, open and resume independently stored projects."));
    ImGui::BeginDisabled(session_->playing()||session_->building()||session_->context().importing());
    drawPathInput(ui::tr("Project document"),projectOpenPath_,"project.open",false,{"azureproject"});
    if(ui::button(ui::tr("Open project"),{ui::ButtonVariant::Primary,false,projectOpenPath_.empty()}))
        session_->edit("project.open",{{"path",projectOpenPath_}});
    observeWidget("project.open");
    ImGui::Separator();ui::sectionHeading(ui::tr("New project"));
    ui::combo(ui::tr("Template"),&projectTemplate_,"Scene inspection\0Playable game\0");observeWidget("project.template");
    ui::inputText("Name",projectName_);observeWidget("project.name");
    drawPathInput(ui::tr("Parent directory"),projectDestination_,"project.create",true);
    ImGui::TextWrapped(ui::tr("The project is created in a new subdirectory named after the project."));
    const bool valid=!projectName_.empty()&&!projectDestination_.empty()&&
        projectName_.find_first_of("/\\:")==std::string::npos&&projectName_!="."&&projectName_!="..";
    ImGui::BeginDisabled(!valid);
    if(ui::button(ui::tr("Create project")))session_->edit("project.create",{{"templateId",projectTemplate_==0?"scene":"game"},
        {"destination",(std::filesystem::u8path(projectDestination_)/std::filesystem::u8path(projectName_)).u8string()},{"name",projectName_}});
    observeWidget("project.create");ImGui::EndDisabled();
    ImGui::Separator();ui::sectionHeading(ui::tr("Recent projects"));
    for(const auto& project:session_->projects().recent()){
        const auto path=project.at("path").get<std::string>();ImGui::PushID(path.c_str());
        const bool available=project.at("available").get<bool>();
        ImGui::BeginDisabled(!available);
        if(ImGui::Selectable(project.at("name").get<std::string>().c_str()))session_->edit("project.open",{{"path",path}});
        observeWidget("project.recent."+project.at("id").get<std::string>());ImGui::EndDisabled();
        ImGui::TextWrapped("%s",path.c_str());
        if(!available){
            ImGui::TextDisabled(ui::tr("Project unavailable. Locate its document to recover it."));
            if(ui::button(ui::tr("Locate document"))) {
                const auto result=session_->edit("path.choose",{{"purpose","project.open"},{"title",ui::tr("Locate project document")},{"directory",false},{"extensions",{".azureproject"}}});
                if(result&&!result.value.value("cancelled",true)) {
                    projectOpenPath_=result.value.at("path").get<std::string>();
                    session_->edit("project.open",{{"path",projectOpenPath_}});
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();ImGui::End();
}
}
#endif
