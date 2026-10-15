#include "ui/localization/Localization.hpp"
#include "editor/panels/CorePanels.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawAssetTransferPanel(){
    if(assetPanel_->presentation.exportDialogRequested_){assetPanel_->presentation.exportDialogRequested_=false;assetPanel_->presentation.exportImpact_=nullptr;assetPanel_->presentation.exportFormat_.clear();assetPanel_->presentation.exportTask_.clear();assetPanel_->presentation.exportReplace_=false;assetPanel_->presentation.exportDestination_=(session_->context().project().file.parent_path().parent_path()/"exchange").u8string();ImGui::OpenPopup(ui::tr("Asset export"));}
    ImGui::SetNextWindowSize({600*dpi_,0},ImGuiCond_Always);
    if(!ImGui::BeginPopupModal(ui::tr("Asset export"),nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::TextUnformatted(assetPanel_->presentation.exportRequest_.contains("node")?ui::tr("Export selected subtree"):"Export selected asset");
    const auto selector=assetPanel_->presentation.exportRequest_.contains("node")?nlohmann::json{{"node",assetPanel_->presentation.exportRequest_.at("node")}}:nlohmann::json{{"asset",assetPanel_->presentation.exportRequest_.at("asset")}};
    const auto discovered=session_->edit("asset.formats",selector);
    if(discovered&&!discovered.value.empty()){
        const auto& formats=discovered.value;if(assetPanel_->presentation.exportFormat_.empty()){assetPanel_->presentation.exportFormat_=formats.front().at("id");assetPanel_->presentation.exportRequest_["settings"]=nlohmann::json::object();}
        ImGui::BeginDisabled(!assetPanel_->presentation.exportTask_.empty());
        if(ImGui::BeginCombo(ui::tr("Format"),assetPanel_->presentation.exportFormat_.c_str())){for(const auto& format:formats){const auto id=format.at("id").get<std::string>();if(ImGui::Selectable(format.at("label").get<std::string>().c_str(),id==assetPanel_->presentation.exportFormat_)){assetPanel_->presentation.exportFormat_=id;assetPanel_->presentation.exportRequest_["settings"]=nlohmann::json::object();assetPanel_->presentation.exportImpact_=nullptr;}observeWidget("export.format."+id);}ImGui::EndCombo();}observeWidget("export.format");
        for(const auto& format:formats)if(format.at("id")==assetPanel_->presentation.exportFormat_){const auto fields=format.at("exportSettings").value("properties",nlohmann::json::object());
            for(auto field=fields.begin();field!=fields.end();++field){const auto& descriptor=field.value();auto& value=assetPanel_->presentation.exportRequest_["settings"][field.key()];if(value.is_null())value=descriptor.at("default");bool changed=false;
                if(descriptor.at("type")=="integer"){int integer=value.get<int>();changed=ImGui::InputInt(field.key().c_str(),&integer);if(changed)value=integer;}
                else if(descriptor.at("type")=="number"){double number=value.get<double>();changed=ImGui::InputDouble(field.key().c_str(),&number);if(changed)value=number;}
                else if(descriptor.at("type")=="string"&&descriptor.contains("enum")){
                    if(ImGui::BeginCombo(field.key().c_str(),value.get_ref<const std::string&>().c_str())){for(const auto& option:descriptor.at("enum")){const auto text=option.get<std::string>();if(ImGui::Selectable(text.c_str(),value==option)){value=option;changed=true;}observeWidget("export.setting."+field.key()+"."+text);}ImGui::EndCombo();}
                }
                if(changed)assetPanel_->presentation.exportImpact_=nullptr;observeWidget("export.setting."+field.key());}}
        const auto destinationBefore=assetPanel_->presentation.exportDestination_;drawPathInput(ui::tr("Output directory"),assetPanel_->presentation.exportDestination_,"export",true);if(assetPanel_->presentation.exportDestination_!=destinationBefore)assetPanel_->presentation.exportImpact_=nullptr;
        if(ui::button(ui::tr("Preview export"))){auto request=assetPanel_->presentation.exportRequest_;request["format"]=assetPanel_->presentation.exportFormat_;request["destination"]=assetPanel_->presentation.exportDestination_;const auto result=session_->edit("asset.export-plan",request);if(result)assetPanel_->presentation.exportImpact_=result.value;}observeWidget("export.plan");
        if(!assetPanel_->presentation.exportImpact_.is_null()){
            ImGui::TextWrapped(ui::tr("Source: %s"),assetPanel_->presentation.exportImpact_.at("source").get<std::string>().c_str());ImGui::TextWrapped(ui::tr("Output: %s"),assetPanel_->presentation.exportImpact_.at("entry").get<std::string>().c_str());
            for(const auto& loss:assetPanel_->presentation.exportImpact_.at("losses"))ImGui::TextWrapped(ui::tr("Conversion: %s"),loss.get<std::string>().c_str());
            for(const auto& dependency:assetPanel_->presentation.exportImpact_.at("dependencies"))ImGui::TextWrapped(ui::tr("Dependency: %s"),dependency.get<std::string>().c_str());
            ImGui::Checkbox(ui::tr("Replace existing output directory"),&assetPanel_->presentation.exportReplace_);observeWidget("export.replace");
            if(ui::button(ui::tr("Start export"))){const auto result=session_->edit("asset.export",{{"plan",assetPanel_->presentation.exportImpact_.at("plan")},{"replace",assetPanel_->presentation.exportReplace_}});if(result)assetPanel_->presentation.exportTask_=result.value.at("task");else assetPanel_->presentation.exportImpact_=nullptr;}observeWidget("export.start");
        }ImGui::EndDisabled();
    }else ImGui::TextWrapped(ui::tr("Select an asset with a registered output format."));
    if(!assetPanel_->presentation.exportTask_.empty())for(const auto& task:session_->tasks().report())if(task.at("id")==assetPanel_->presentation.exportTask_){
        ImGui::Text(ui::tr("Task: %s"),task.at("state").get<std::string>().c_str());ImGui::ProgressBar(task.at("progress").get<float>());
        if(task.at("state")=="running"){if(ui::button(ui::tr("Cancel export")))session_->edit("tasks.cancel",{{"id",assetPanel_->presentation.exportTask_}});observeWidget("export.cancel");}
        else{if(!task.at("diagnostic").get<std::string>().empty())ImGui::TextWrapped("%s",task.at("diagnostic").get<std::string>().c_str());if(task.at("state")=="completed")ImGui::TextWrapped(ui::tr("Saved: %s"),task.at("result").at("output").get<std::string>().c_str());if(ui::button(ui::tr("Plan again"))){assetPanel_->presentation.exportTask_.clear();assetPanel_->presentation.exportImpact_=nullptr;}observeWidget("export.retry");}}
    if(ui::button(ui::tr("Close")))ImGui::CloseCurrentPopup();observeWidget("export.close");ImGui::EndPopup();
}
}
#endif
