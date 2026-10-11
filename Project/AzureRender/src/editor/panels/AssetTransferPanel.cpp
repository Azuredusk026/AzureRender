#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawAssetTransferPanel(){
    if(exportDialogRequested_){exportDialogRequested_=false;exportImpact_=nullptr;exportFormat_.clear();exportTask_.clear();exportReplace_=false;exportDestination_=(session_->context().project().file.parent_path().parent_path()/"exchange").u8string();ImGui::OpenPopup("Asset export");}
    ImGui::SetNextWindowSize({600*dpi_,0},ImGuiCond_Always);
    if(!ImGui::BeginPopupModal("Asset export",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::TextUnformatted(exportRequest_.contains("node")?"Export selected subtree":"Export selected asset");
    const auto selector=exportRequest_.contains("node")?nlohmann::json{{"node",exportRequest_.at("node")}}:nlohmann::json{{"asset",exportRequest_.at("asset")}};
    const auto discovered=session_->edit("asset.formats",selector);
    if(discovered&&!discovered.value.empty()){
        const auto& formats=discovered.value;if(exportFormat_.empty()){exportFormat_=formats.front().at("id");exportRequest_["settings"]=nlohmann::json::object();}
        ImGui::BeginDisabled(!exportTask_.empty());
        if(ImGui::BeginCombo("Format",exportFormat_.c_str())){for(const auto& format:formats){const auto id=format.at("id").get<std::string>();if(ImGui::Selectable(format.at("label").get<std::string>().c_str(),id==exportFormat_)){exportFormat_=id;exportRequest_["settings"]=nlohmann::json::object();exportImpact_=nullptr;}observeWidget("export.format."+id);}ImGui::EndCombo();}observeWidget("export.format");
        for(const auto& format:formats)if(format.at("id")==exportFormat_){const auto fields=format.at("exportSettings").value("properties",nlohmann::json::object());
            for(auto field=fields.begin();field!=fields.end();++field){const auto& descriptor=field.value();auto& value=exportRequest_["settings"][field.key()];if(value.is_null())value=descriptor.at("default");bool changed=false;
                if(descriptor.at("type")=="integer"){int integer=value.get<int>();changed=ImGui::InputInt(field.key().c_str(),&integer);if(changed)value=integer;}
                else if(descriptor.at("type")=="number"){double number=value.get<double>();changed=ImGui::InputDouble(field.key().c_str(),&number);if(changed)value=number;}
                else if(descriptor.at("type")=="string"&&descriptor.contains("enum")){
                    if(ImGui::BeginCombo(field.key().c_str(),value.get_ref<const std::string&>().c_str())){for(const auto& option:descriptor.at("enum")){const auto text=option.get<std::string>();if(ImGui::Selectable(text.c_str(),value==option)){value=option;changed=true;}observeWidget("export.setting."+field.key()+"."+text);}ImGui::EndCombo();}
                }
                if(changed)exportImpact_=nullptr;observeWidget("export.setting."+field.key());}}
        const auto destinationBefore=exportDestination_;drawPathInput("Output directory",exportDestination_,"export",true);if(exportDestination_!=destinationBefore)exportImpact_=nullptr;
        if(ui::button("Preview export")){auto request=exportRequest_;request["format"]=exportFormat_;request["destination"]=exportDestination_;const auto result=session_->edit("asset.export-plan",request);if(result)exportImpact_=result.value;}observeWidget("export.plan");
        if(!exportImpact_.is_null()){
            ImGui::TextWrapped("Source: %s",exportImpact_.at("source").get<std::string>().c_str());ImGui::TextWrapped("Output: %s",exportImpact_.at("entry").get<std::string>().c_str());
            for(const auto& loss:exportImpact_.at("losses"))ImGui::TextWrapped("Conversion: %s",loss.get<std::string>().c_str());
            for(const auto& dependency:exportImpact_.at("dependencies"))ImGui::TextWrapped("Dependency: %s",dependency.get<std::string>().c_str());
            ImGui::Checkbox("Replace existing output directory",&exportReplace_);observeWidget("export.replace");
            if(ui::button("Start export")){const auto result=session_->edit("asset.export",{{"plan",exportImpact_.at("plan")},{"replace",exportReplace_}});if(result)exportTask_=result.value.at("task");else exportImpact_=nullptr;}observeWidget("export.start");
        }ImGui::EndDisabled();
    }else ImGui::TextWrapped("Select an asset with a registered output format.");
    if(!exportTask_.empty())for(const auto& task:session_->tasks().report())if(task.at("id")==exportTask_){
        ImGui::Text("Task: %s",task.at("state").get<std::string>().c_str());ImGui::ProgressBar(task.at("progress").get<float>());
        if(task.at("state")=="running"){if(ui::button("Cancel export"))session_->edit("tasks.cancel",{{"id",exportTask_}});observeWidget("export.cancel");}
        else{if(!task.at("diagnostic").get<std::string>().empty())ImGui::TextWrapped("%s",task.at("diagnostic").get<std::string>().c_str());if(task.at("state")=="completed")ImGui::TextWrapped("Saved: %s",task.at("result").at("output").get<std::string>().c_str());if(ui::button("Plan again")){exportTask_.clear();exportImpact_=nullptr;}observeWidget("export.retry");}}
    if(ui::button("Close"))ImGui::CloseCurrentPopup();observeWidget("export.close");ImGui::EndPopup();
}
}
#endif
