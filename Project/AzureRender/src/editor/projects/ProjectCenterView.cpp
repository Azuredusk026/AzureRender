#include "ProjectCenterView.hpp"
#include "editor/ui/Widgets.hpp"
#include "platform/PathSelection.hpp"
#include <imgui.h>
namespace azurerender {
void ProjectCenterView::draw(ProjectCenterController& controller) {
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos); ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("AzureRender Project Center",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("AzureRender"); ImGui::TextDisabled("Project Center / 项目中心"); ImGui::Separator();
    if(controller.busy()) { ImGui::TextWrapped("%s",controller.operation().c_str()); ImGui::TextDisabled("Preparing project. The editor starts after validation completes."); }
    ui::resultMessage(controller.diagnostic(),true);ui::resultMessage(diagnostic_,true);
    ImGui::BeginDisabled(controller.busy());
    if(ImGui::BeginTabBar("Project routes")) {
        if(ImGui::BeginTabItem("Recent / 最近项目")) {
            for(const auto& project:controller.recent()) {
                ImGui::PushID(project.at("path").get_ref<const std::string&>().c_str());
                ImGui::TextUnformatted(project.at("name").get_ref<const std::string&>().c_str());
                ImGui::TextWrapped("%s",project.at("path").get_ref<const std::string&>().c_str());
                const bool available=project.at("available");
                ui::ButtonOptions open;open.disabled=!available;
                if(ui::button("Open project",open)) controller.open(std::filesystem::u8path(project.at("path").get<std::string>()));
                ImGui::SameLine(); if(ui::button("Locate document")) {
                    const auto result=choosePath({"Locate project",false,{}, {".azureproject"}});
                    if(!result.path.empty())controller.open(result.path);diagnostic_=result.diagnostic;
                }
                if(!available)ui::resultMessage(project.value("diagnostic",std::string("Unavailable project")),true);
                ImGui::Separator();ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Open / 打开项目")) {
            ui::inputText("Project document",projectPath_);
            if(ui::button("Browse project")) {const auto r=choosePath({"Open project",false,{}, {".azureproject"}});if(!r.path.empty())projectPath_=r.path.u8string();diagnostic_=r.diagnostic;}
            ui::ButtonOptions open;open.disabled=projectPath_.empty();open.variant=ui::ButtonVariant::Primary;
            if(ui::button("Open project",open))controller.open(std::filesystem::u8path(projectPath_));
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("New / 新建项目")) {
            ui::inputText("Project name",name_);ui::inputText("Parent directory",parent_);
            if(ui::button("Browse directory")){const auto r=choosePath({"Choose projects directory",true});if(!r.path.empty())parent_=r.path.u8string();diagnostic_=r.diagnostic;}
            ImGui::Combo("Template",&template_,"Scene inspection\0Playable game\0");
            ui::ButtonOptions create;create.variant=ui::ButtonVariant::Primary;create.disabled=name_.empty()||parent_.empty();
            if(ui::button("Create project",create)) {
                const auto component=std::filesystem::u8path(name_);
                if(component.has_parent_path()||component=="."||component=="..")diagnostic_="Project name must be a directory name";
                else controller.create(template_==0?"scene":"game",std::filesystem::u8path(parent_)/component,name_);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndDisabled();
    if(!projectPath_.empty() && ui::button("Reveal project directory")) {try {revealPath(std::filesystem::u8path(projectPath_));}catch(const std::exception& e){diagnostic_=e.what();}}
    ImGui::End();
}
}
