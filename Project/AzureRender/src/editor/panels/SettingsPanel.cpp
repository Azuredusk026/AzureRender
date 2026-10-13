#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#include "runtime/ProjectSettings.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <cstring>
namespace azurerender {
void ImGuiEditorLayer::drawSettingsPanel(PanelContext&) {
    if(!ImGui::Begin("Settings###settings",workspace_.open("settings"))){ImGui::End();return;}
    const char* categories[]={"编辑器偏好","项目设置","渲染诊断","内容辅助"};
    if(ImGui::BeginCombo("Category",categories[settingsCategory_])){
        for(int category=0;category<4;++category){
            const bool selected=settingsCategory_==category;
            if(ImGui::Selectable(categories[category],selected))settingsCategory_=category;
            observeWidget("settings.category."+std::to_string(category));
            if(selected)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    observeWidget("settings.category");
    if(settingsCategory_==1){
        if(session_->context().isProject()){
            const auto& project=session_->context().project();
            ImGui::TextWrapped("Project: %s",project.name.c_str());ImGui::TextWrapped("Startup: %s",project.startupScene.c_str());
            ImGui::TextWrapped("Configuration: %s",project.file.u8string().c_str());
            ImGui::TextWrapped("保存位置：%s",projectSettingsPath(project).u8string().c_str());
        }else ImGui::TextUnformatted("Open a project to inspect its configuration.");
        if(!session_->context().isProject()){ImGui::End();return;}
    }
    settingsFilter_.Draw("Search settings",-1);observeWidget("settings.search");
    auto& registry=session_->settings();const auto descriptors=registry.describe();
    const char* sources[]={"Console override","User preference"};ImGui::Combo("Write source",&settingSourceIndex_,sources,2);observeWidget("settings.source");
    const auto source=settingsCategory_==1?SettingSource::Project:settingSourceIndex_==0?SettingSource::Console:SettingSource::UserFile;
    ImGui::TextWrapped("覆盖顺序：默认、安装配置、个人偏好、项目、启动参数、临时覆盖。");
    for(const auto& field:descriptors.items()) {
        const bool editor=field.key().rfind("editor.",0)==0;
        const bool ai=field.key().rfind("ai.",0)==0;
        if((settingsCategory_==0&&!editor)||(settingsCategory_==1&&!projectSetting(field.key()))||(settingsCategory_==3&&!ai)||(settingsCategory_==2&&(editor||ai)))continue;
        if(!settingsFilter_.PassFilter(field.key().c_str()))continue;
        ui::ScopedId id(field.key().c_str());const auto& d=field.value();auto value=d.at("pending");
        ImGui::TextWrapped("%s",d.at("description").get<std::string>().c_str());ImGui::TextDisabled("%s | %s",field.key().c_str(),d.at("source").get<std::string>().c_str());
        if(d.at("startupOnly").get<bool>())ImGui::TextDisabled("启动时生效");
        bool changed=false;
        {ui::ScopedDisabled disabled(d.at("readOnly").get<bool>()||d.at("startupOnly").get<bool>());
        ui::propertyRow("Value",ui::UiMetrics::fromScale(dpi_),[&] {
            if(value.is_boolean()){bool v=value.get<bool>();changed=ImGui::Checkbox("##value",&v);value=v;}
            else if(value.is_number_integer()){std::int64_t v=value.get<std::int64_t>();changed=ImGui::InputScalar("##value",ImGuiDataType_S64,&v);value=v;}
            else if(value.is_number()){float v=value.get<float>();changed=ImGui::InputFloat("##value",&v,0,0,"%.3f");value=v;}
            else {std::array<char,1024> buffer{};const auto text=value.get<std::string>();std::memcpy(buffer.data(),text.data(),std::min(text.size(),buffer.size()-1));changed=ImGui::InputText("##value",buffer.data(),buffer.size());value=buffer.data();}
            observeWidget("setting."+field.key());
        });
        if(changed){const auto result=session_->edit("settings.set",{{"name",field.key()},{"value",value},{"source",settingSourceName(source)}});settingDiagnostic_=result?std::string():session_->lastError();}
        if(ui::button("Reset source")){const auto result=session_->edit("settings.reset",{{"name",field.key()},{"source",settingSourceName(source)}});settingDiagnostic_=result?std::string():session_->lastError();}
        observeWidget("setting.reset."+field.key());}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",d.at("description").get<std::string>().c_str());
        ImGui::Separator();
    }
    if(ui::button(settingsCategory_==1?"保存项目设置":"保存个人偏好")){
        if(settingsCategory_==1){const auto result=session_->edit("settings.project-save");settingDiagnostic_=result?std::string():session_->lastError();}
        else settingsSaveRequested_=true;
    }
    observeWidget("settings.save");ui::resultMessage(settingDiagnostic_,true);ImGui::End();
}
}
#endif
