#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
#include "runtime/ProjectSettings.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <cstring>
namespace azurerender {
void ImGuiEditorLayer::drawSettingsPanel(PanelContext&) {
    if(!ImGui::Begin("设置###settings",workspace_.open("settings"))){ImGui::End();return;}
    const char* categories[]={"编辑器偏好","项目设置","渲染诊断","内容辅助"};
    if(ImGui::BeginCombo("设置分类",categories[workspace_.settingsCategory])){
        for(int category=0;category<4;++category){
            const bool selected=workspace_.settingsCategory==category;
            if(ImGui::Selectable(categories[category],selected))workspace_.settingsCategory=category;
            observeWidget("settings.category."+std::to_string(category));
            if(selected)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    observeWidget("settings.category");
    if(workspace_.settingsCategory==1){
        if(session_->context().isProject()){
            const auto& project=session_->context().project();
            ImGui::TextWrapped("项目：%s",project.name.c_str());ImGui::TextWrapped("启动场景：%s",project.startupScene.c_str());
            ImGui::TextWrapped("项目文档：%s",project.file.u8string().c_str());
            ImGui::TextWrapped("保存位置：%s",projectSettingsPath(project).u8string().c_str());
        }else ImGui::TextUnformatted("打开项目后可编辑项目设置。");
        if(!session_->context().isProject()){ImGui::End();return;}
    }
    ui::sectionHeading(categories[workspace_.settingsCategory],workspace_.settingsCategory==1?"设置随项目保存。":workspace_.settingsCategory==2?"诊断只影响当前运行会话。":"偏好保存在当前用户配置目录。");
    ui::searchField("##settings-search","搜索名称或说明",settingsFilter_);observeWidget("settings.search");
    auto& registry=session_->settings();const auto descriptors=registry.describe();
    const auto source=workspace_.settingsCategory==1?SettingSource::Project:workspace_.settingsCategory==2?SettingSource::Console:SettingSource::UserFile;
    if(workspace_.settingsCategory==2)ImGui::TextWrapped("诊断设置仅对当前会话生效。");
    for(const auto& field:descriptors.items()) {
        if(!field.value().value("editorVisible",true))continue;
        const bool editor=field.key().rfind("editor.",0)==0;
        const bool ai=field.key().rfind("ai.",0)==0;
        if((workspace_.settingsCategory==0&&!editor)||(workspace_.settingsCategory==1&&!projectSetting(field.key()))||(workspace_.settingsCategory==3&&!ai)||(workspace_.settingsCategory==2&&(editor||ai||projectSetting(field.key()))))continue;
        if(!settingsFilter_.PassFilter(field.key().c_str())&&!settingsFilter_.PassFilter(field.value().at("description").get_ref<const std::string&>().c_str()))continue;
        ui::ScopedId id(field.key().c_str());const auto& d=field.value();auto value=d.at("pending");
        ImGui::TextWrapped("%s",d.at("description").get<std::string>().c_str());if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",field.key().c_str());
        if(d.at("startupOnly").get<bool>())ImGui::TextDisabled("启动时生效");
        bool changed=false;
        {ui::ScopedDisabled disabled(d.at("readOnly").get<bool>()||d.at("startupOnly").get<bool>());
        ui::propertyRow("数值",ui::UiMetrics::fromScale(dpi_),[&] {
            if(value.is_boolean()){bool v=value.get<bool>();changed=ImGui::Checkbox("##value",&v);value=v;}
            else if(value.is_number_integer()){std::int64_t v=value.get<std::int64_t>();changed=ImGui::InputScalar("##value",ImGuiDataType_S64,&v);value=v;}
            else if(value.is_number()){float v=value.get<float>();changed=ImGui::InputFloat("##value",&v,0,0,"%.3f");value=v;}
            else {std::array<char,1024> buffer{};const auto text=value.get<std::string>();std::memcpy(buffer.data(),text.data(),std::min(text.size(),buffer.size()-1));changed=ImGui::InputText("##value",buffer.data(),buffer.size());value=buffer.data();}
            observeWidget("setting."+field.key());
        });
        if(changed){const auto result=session_->edit("settings.set",{{"name",field.key()},{"value",value},{"source",settingSourceName(source)}});settingDiagnostic_=result?std::string():session_->lastError();}
        if(ui::button("恢复默认值")){const auto result=session_->edit("settings.reset",{{"name",field.key()},{"source",settingSourceName(source)}});settingDiagnostic_=result?std::string():session_->lastError();}
        observeWidget("setting.reset."+field.key());}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",d.at("description").get<std::string>().c_str());
        ImGui::Separator();
    }
    if(workspace_.settingsCategory!=2&&ui::button(workspace_.settingsCategory==1?"保存项目设置":"保存个人偏好")){
        if(workspace_.settingsCategory==1){const auto result=session_->edit("settings.project-save");settingDiagnostic_=result?std::string():session_->lastError();}
        else settingsSaveRequested_=true;
    }
    observeWidget("settings.save");ui::resultMessage(settingDiagnostic_,true);ImGui::End();
}
}
#endif
