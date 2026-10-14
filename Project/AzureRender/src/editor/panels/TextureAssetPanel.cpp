#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawTextureAssetPanel() {
    if(contentDialogRequested_){ImGui::OpenPopup("Content request");contentDialogRequested_=false;}
    if(textureDialogRequested_){ImGui::OpenPopup("Texture settings");textureDialogRequested_=false;}
    if(dependencyDialogRequested_){ImGui::OpenPopup("Asset dependencies");dependencyDialogRequested_=false;}
    if(ImGui::BeginPopupModal("Content request",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(contentOperation_.c_str());
        ui::inputText("Name / destination",contentValue_);observeWidget("content.value");
        if(ui::button("Preview impact")) {
            auto request=contentRequest_;request["dryRun"]=true;
            if(contentOperation_=="content.create-directory"||contentOperation_=="asset.rename")request["name"]=contentValue_;
            else request["destination"]=contentValue_;
            const auto result=session_->edit(contentOperation_,request);
            if(result){contentImpact_=result.value;ImGui::CloseCurrentPopup();}
        }observeWidget("content.plan");ImGui::SameLine();
        if(ui::button("Cancel"))ImGui::CloseCurrentPopup();observeWidget("content.cancel-request");
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal("Texture settings",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        widgets_["texture.dialog"]={position.x,position.y,size.x,size.y};
        ImGui::Text("%zu image asset(s)",textureSnapshot_.at("assets").size());
        ImGui::TextWrapped("Choose the fields to apply. Mixed fields keep each asset's value until selected.");
        auto& values=textureSnapshot_["values"];
        const std::map<std::string,std::vector<const char*>> choices{
            {"usage",{"albedo","emissive","normal","mask","roughness","linear","packed-normal"}},
            {"colorSpace",{"srgb","linear"}},{"filter",{"linear","nearest"}},
            {"wrapU",{"repeat","clamp","mirror"}},{"wrapV",{"repeat","clamp","mirror"}},
            {"mipPolicy",{"generate","none"}}};
        for(const auto& [field,options]:choices) {
            ImGui::PushID(field.c_str());bool enabled=textureFields_.count(field)!=0;
            if(ImGui::Checkbox("##apply",&enabled)){if(enabled)textureFields_.insert(field);else textureFields_.erase(field);}observeWidget("texture.enable."+field);
            ImGui::SameLine();const auto current=values.at(field).get<std::string>();
            const bool mixed=std::find(textureSnapshot_["mixed"].begin(),textureSnapshot_["mixed"].end(),nlohmann::json(field))!=textureSnapshot_["mixed"].end();
            ImGui::BeginDisabled(!enabled);ImGui::SetNextItemWidth(170*dpi_);
            if(ImGui::BeginCombo(field.c_str(),mixed&&!enabled?"Mixed":current.c_str())) {
                for(const auto* option:options) {
                    if(ImGui::Selectable(option,current==option))values[field]=option;
                    observeWidget("texture.option."+field+"."+option);
                }ImGui::EndCombo();
            }observeWidget("texture."+field);ImGui::EndDisabled();ImGui::PopID();
        }
        bool anisotropy=textureFields_.count("anisotropy")!=0;
        if(ImGui::Checkbox("##anisotropy-apply",&anisotropy)){if(anisotropy)textureFields_.insert("anisotropy");else textureFields_.erase("anisotropy");}
        observeWidget("texture.enable.anisotropy");ImGui::SameLine();ImGui::BeginDisabled(!anisotropy);
        float amount=values.at("anisotropy").get<float>();if(ImGui::SliderFloat("Anisotropy",&amount,1,16,"%.0f"))values["anisotropy"]=amount;
        observeWidget("texture.anisotropy");ImGui::EndDisabled();
        ui::ButtonOptions apply;apply.disabled=textureFields_.empty()||session_->context().dirty()||session_->reimporting();
        if(ui::button("Preview impact",apply)) {
            auto fields=nlohmann::json::object(),assets=nlohmann::json::array();
            for(const auto& field:textureFields_)fields[field]=values.at(field);
            for(const auto& asset:textureSnapshot_.at("assets"))assets.push_back(asset.at("id"));
            const auto result=session_->edit("texture.settings",{{"assets",assets},{"values",fields},{"dryRun",true}});
            if(result){contentOperation_="texture.settings";contentImpact_=result.value;ImGui::CloseCurrentPopup();}
        }observeWidget("texture.plan");ImGui::SameLine();
        if(ui::button("Cancel"))ImGui::CloseCurrentPopup();observeWidget("texture.cancel");
        ImGui::EndPopup();
    }
    if(!contentImpact_.is_null()&&!contentImpact_.empty()&&!ImGui::IsPopupOpen("Content impact"))ImGui::OpenPopup("Content impact");
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal("Content impact",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Apply %s to the following files:",contentOperation_.c_str());
        ImGui::BeginChild("Affected files",{500*dpi_,180*dpi_});
        for(const auto& path:contentImpact_.at("affected"))ImGui::TextUnformatted(path.get<std::string>().c_str());
        ImGui::EndChild();
        ui::ButtonOptions apply;apply.disabled=session_->context().dirty()||session_->reimporting();
        apply.tooltip="Save scene changes before publishing content operations.";
        if(ui::button("Apply",apply)) {
            const auto result=session_->edit(contentOperation_,{{"planId",contentImpact_.at("id")}});
            if(result){contentImpact_=nullptr;selectedAssets_.clear();ImGui::CloseCurrentPopup();}
        }observeWidget("content.apply");ImGui::SameLine();
        if(ui::button("Cancel")){session_->edit("content.discard",{{"planId",contentImpact_.at("id")}});contentImpact_=nullptr;ImGui::CloseCurrentPopup();}
        observeWidget("content.cancel-impact");ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal("Asset dependencies",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        if(dependencySnapshot_.contains("path")) {
            ImGui::TextWrapped("%s",dependencySnapshot_.at("path").get<std::string>().c_str());
            ImGui::Text("Type: %s",dependencySnapshot_.at("type").get<std::string>().c_str());
            ImGui::Text("Source: %s",dependencySnapshot_.at("ready").get<bool>()?"Available":"Missing");
            if(dependencySnapshot_.value("sourceChanged",false)||dependencySnapshot_.value("activeChanged",false))ImGui::TextWrapped("Source or active content changed. Validate with Reimport or Reload Assets.");
            for(const auto* field:{"dependencies","referencedBy"})if(ImGui::CollapsingHeader(field))
                for(const auto& row:dependencySnapshot_.at(field))ImGui::TextWrapped("%s",row.at("path").get<std::string>().c_str());
            for(const auto* field:{"id","fingerprint","source","settings","generation","importGeneration","diagnostic"})if(dependencySnapshot_.contains(field)&&ImGui::CollapsingHeader(field))
                ImGui::TextWrapped("%s",dependencySnapshot_.at(field).dump(2).c_str());
        }else {
            ImGui::TextWrapped("%s dependencies:",dependencySnapshot_.at("direction").get<std::string>().c_str());
            for(const auto& id:dependencySnapshot_.at("assets")) {
                const auto& record=session_->context().assets().records().at(id.get<std::string>());
                ImGui::TextWrapped("%s",record.virtualPath.c_str());
            }
        }
        if(ui::button("Close"))ImGui::CloseCurrentPopup();observeWidget("content.close-dependencies");ImGui::EndPopup();
    }
}
}
#endif
