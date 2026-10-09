#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <cstring>
namespace azurerender {
void ImGuiEditorLayer::drawSettingsPanel(PanelContext&) {
    if(!ImGui::Begin("Settings###settings",workspace_.open("settings"))){ImGui::End();return;}
    const char* categories[]={"Editor preferences","Project configuration","Renderer diagnostics","Content assistance"};
    ImGui::Combo("Category",&settingsCategory_,categories,4);observeWidget("settings.category");
    if(settingsCategory_==1){
        if(session_->context().isProject()){
            const auto& project=session_->context().project();
            ImGui::TextWrapped("Project: %s",project.name.c_str());ImGui::TextWrapped("Startup: %s",project.startupScene.c_str());
            ImGui::TextWrapped("Configuration: %s",project.file.u8string().c_str());
            ImGui::TextWrapped("Runtime systems and input bindings belong to the project document.");
        }else ImGui::TextUnformatted("Open a project to inspect its configuration.");
        ImGui::End();return;
    }
    settingsFilter_.Draw("Search settings",-1);observeWidget("settings.search");
    auto& registry=session_->settings();const auto descriptors=registry.describe();
    const char* sources[]={"Console override","User preference"};ImGui::Combo("Write source",&settingSourceIndex_,sources,2);observeWidget("settings.source");
    const auto source=settingSourceIndex_==0?SettingSource::Console:SettingSource::UserFile;
    for(const auto& field:descriptors.items()) {
        const bool editor=field.key().rfind("editor.",0)==0;
        const bool ai=field.key().rfind("ai.",0)==0;
        if((settingsCategory_==0&&!editor)||(settingsCategory_==3&&!ai)||(settingsCategory_==2&&(editor||ai)))continue;
        if(!settingsFilter_.PassFilter(field.key().c_str()))continue;
        ui::ScopedId id(field.key().c_str());const auto& d=field.value();auto value=d.at("pending");
        ImGui::TextUnformatted(field.key().c_str());ImGui::TextDisabled("%s",d.at("source").get<std::string>().c_str());
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
    if(ui::button("Save user preferences"))settingsSaveRequested_=true;
    observeWidget("settings.save");ui::resultMessage(settingDiagnostic_,true);ImGui::End();
}
}
#endif
