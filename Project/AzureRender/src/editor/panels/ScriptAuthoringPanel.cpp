#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawScriptAuthoringPanel(PanelContext& panel){
    if(!ImGui::Begin("脚本制作###scripts",workspace_.open("scripts"))){ImGui::End();return;}
    if(!panel.view().isProject()){ImGui::TextUnformatted("打开项目后可创建与附加脚本。");ImGui::End();return;}
    ui::sectionHeading("脚本制作","创建脚本、打开源文件，并将逻辑附加到选中物体。");
    ImGui::BeginDisabled(session_->playing()||session_->building());
    if(ImGui::BeginCombo("脚本语言",scriptTemplateBackend_.c_str())){
        for(const auto& backend:session_->scriptAuthoring().describe()){
            const auto id=backend.at("id").get<std::string>();
            if(ImGui::Selectable(backend.at("label").get<std::string>().c_str(),id==scriptTemplateBackend_)){scriptTemplateBackend_=id;scriptTemplatePath_="assets:/NewBehaviour"+backend.at("extension").get<std::string>();}
            observeWidget("script.backend."+id);
        }ImGui::EndCombo();
    }observeWidget("script.backend");
    ImGui::SetNextItemWidth(-1);ui::inputText("项目内脚本路径",scriptTemplatePath_);observeWidget("script.template-path");
    if(ui::button("创建脚本")){const auto result=session_->edit("script.template",{{"backend",scriptTemplateBackend_},{"path",scriptTemplatePath_}});if(result)session_->edit("script.reveal",{{"asset",result.value.at("asset")}});}observeWidget("script.create");
    const auto* owner=panel.view().selectedNode();
    ImGui::BeginDisabled(!owner||scriptTemplateBackend_!="lua"||!panel.view().componentData(owner?owner->id:std::string(),"azure.script").is_null());
    if(ui::button("创建并附加到选中物体")){
        const auto node=owner->id;
        const auto result=session_->edit("script.template",{{"backend",scriptTemplateBackend_},{"path",scriptTemplatePath_}});
        if(result)session_->edit("script.attach",{{"node",node},{"asset",result.value.at("asset")}});
    }observeWidget("script.create-attach");ImGui::EndDisabled();
    if(owner)ImGui::TextWrapped("附加目标：%s",owner->name.c_str());
    else ImGui::TextUnformatted("先在层级或场景中选择一个物体。");
    ImGui::EndDisabled();
    if(ui::button("重新加载脚本"))session_->edit("script.reload",{{"backend",scriptTemplateBackend_}});observeWidget("script.reload");
    if(const auto* selected=panel.view().selectedNode()){
        for(const auto* type:{"azure.script","azure.managed-script"}){
            const auto component=panel.view().componentData(selected->id,type);
            if(component.is_null())continue;
            const auto asset=component.value("asset",std::string());
            if(asset.empty())continue;
            ImGui::PushID(type);ImGui::TextWrapped("%s",asset.c_str());
            if(ui::button("打开源文件"))session_->edit("script.reveal",{{"asset",asset}});observeWidget(std::string("script.reveal.")+type);ImGui::PopID();
        }
    }
    ImGui::TextWrapped("Lua 脚本可以直接附加。C# 源文件需经托管构建流程生成 .azscript 后附加。");
    ImGui::End();
}
}
#endif
