#include "ProjectCenterView.hpp"
#include "ui/Widgets.hpp"
#include "ui/Icons.hpp"
#include "platform/PathSelection.hpp"
#include "editor/EditorWorkspace.hpp"
#include <imgui.h>
#include <algorithm>
namespace azurerender {
void ProjectCenterView::draw(ProjectCenterController& controller,const std::function<void(const std::string&)>& observe) {
    const auto watch=[&](const std::string& id){if(observe)observe(id);};
    const auto* viewport=ImGui::GetMainViewport();
    const auto scale=ImGui::GetStyle().FontScaleDpi;
    const auto tokens=ui::ThemeTokens::dark();
    ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("AzureRender Project Center",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    const auto width=std::min(920*scale,ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,(viewport->WorkSize.x-width)/2));
    ImGui::BeginChild("Center content",{width,0},ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::Spacing();ImGui::PushFont(nullptr,28*scale);ui::icon(ui::Icon::Project);ImGui::SameLine();ImGui::TextColored(tokens.accent,"AzureRender");ImGui::PopFont();
    ui::sectionHeading("项目中心","创建独立项目，或打开已有项目继续制作。");
    ImGui::Separator();
    if(controller.busy())ui::taskFeedback(controller.operation(),"任务完成后进入编辑器。关闭窗口会等待当前项目事务结束。");
    if(!controller.diagnostic().empty()||!diagnostic_.empty())ui::card("Diagnostics",[&]{ui::resultMessage(controller.diagnostic(),true);ui::resultMessage(diagnostic_,true);});
    ImGui::BeginDisabled(controller.busy());
    if(ImGui::BeginTabBar("Project routes")) {
        const bool recent=ImGui::BeginTabItem("最近项目");watch("center.recent.tab");
        if(recent) {
            const auto projects=controller.recent();
            if(projects.empty())ui::card("Welcome",[]{ui::sectionHeading("开始制作第一个项目","选择“新建项目”创建可编辑工程，或选择“打开项目”浏览项目文档。");});
            for(const auto& project:projects) {
                const auto path=project.at("path").get<std::string>();ImGui::PushID(path.c_str());
                ui::card("Project",[&]{
                    ui::sectionHeading(project.at("name").get_ref<const std::string&>().c_str());
                    ImGui::TextWrapped("%s",path.c_str());
                    const bool available=project.at("available");
                    ImGui::TextDisabled("%s",available?"兼容 · 可打开":"需要重新定位或检查兼容版本");
                    ui::ButtonOptions open;open.disabled=!available;open.variant=ui::ButtonVariant::Primary;
                    if(ui::button("打开",open))controller.open(std::filesystem::u8path(path));watch("center.recent."+project.at("id").get<std::string>());
                    ImGui::SameLine();if(ui::button("重新定位")) {
                        const auto r=choosePath({"定位项目文档",false,{}, {".azureproject"}});
                        if(!r.path.empty())controller.open(r.path);diagnostic_=r.diagnostic;
                    }watch("center.locate."+project.at("id").get<std::string>());
                    if(!available)ui::resultMessage(project.value("diagnostic",std::string("项目不可用")),true);
                });ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        const bool openTab=ImGui::BeginTabItem("打开项目");watch("center.open.tab");
        if(openTab) {
            ui::card("Open",[&]{
                ui::sectionHeading("打开项目","选择 .azureproject 文档。项目目录可以位于任意盘符。");
                ImGui::TextUnformatted("项目文档");ImGui::SetNextItemWidth(-1);ui::inputText("##document",projectPath_);watch("center.open.path");
                if(ui::button("浏览项目")) {const auto r=choosePath({"打开项目",false,{}, {".azureproject"}});if(!r.path.empty())projectPath_=r.path.u8string();diagnostic_=r.diagnostic;}watch("center.open.browse");
                ImGui::SameLine();ui::ButtonOptions open;open.disabled=projectPath_.empty();open.variant=ui::ButtonVariant::Primary;
                if(ui::button("打开项目",open))controller.open(std::filesystem::u8path(projectPath_));watch("center.open");
                if(!projectPath_.empty()&&ui::button("打开所在文件夹")) {try{revealPath(std::filesystem::u8path(projectPath_));}catch(const std::exception& e){diagnostic_=e.what();}}
            });ImGui::EndTabItem();
        }
        const bool newTab=ImGui::BeginTabItem("新建项目");watch("center.new.tab");
        if(newTab) {
            ui::card("New",[&]{
                ui::sectionHeading("新建项目","项目资产、配置与可重建缓存保存在独立工程目录。");
                ImGui::TextUnformatted("项目名称");ImGui::SetNextItemWidth(-1);ui::inputText("##name",name_);watch("center.name");
                ImGui::TextUnformatted("父目录");ImGui::SetNextItemWidth(-1);ui::inputText("##parent",parent_);watch("center.parent");
                if(ui::button("浏览目录")){const auto r=choosePath({"选择项目父目录",true});if(!r.path.empty())parent_=r.path.u8string();diagnostic_=r.diagnostic;}watch("center.parent.browse");
                ImGui::Combo("模板",&template_,"场景检查\0可玩游戏\0");watch("center.template");
                if(!parent_.empty())ImGui::TextWrapped("工程目录：%s",(std::filesystem::u8path(parent_)/std::filesystem::u8path(name_)).u8string().c_str());
                ui::ButtonOptions create;create.variant=ui::ButtonVariant::Primary;create.disabled=name_.empty()||parent_.empty();
                if(ui::button("创建项目",create)) {
                    const auto component=std::filesystem::u8path(name_);
                    if(component.has_parent_path()||component=="."||component==".."||name_.find_first_of("<>:\"/\\|?*")!=std::string::npos)diagnostic_="项目名称应为合法目录名";
                    else controller.create(template_==0?"scene":"game",std::filesystem::u8path(parent_)/component,name_);
                }watch("center.create");
            });ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndDisabled();ImGui::Spacing();
    ImGui::TextDisabled("AzureRender · Windows");ImGui::SameLine();
    if(ui::button("查看启动日志")){try{revealPath(EditorWorkspace::configDirectory()/"logs");}catch(const std::exception& e){diagnostic_=e.what();}}
    ImGui::EndChild();ImGui::End();
}
}
