#include "ui/localization/Localization.hpp"
#include "editor/panels/CorePanels.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawTextureAssetPanel() {
    if(assetPanel_->presentation.contentDialogRequested_){ImGui::OpenPopup(ui::tr("Content request"));assetPanel_->presentation.contentDialogRequested_=false;}
    if(assetPanel_->presentation.textureDialogRequested_){ImGui::OpenPopup(ui::tr("Texture settings"));assetPanel_->presentation.textureDialogRequested_=false;}
    if(assetPanel_->presentation.dependencyDialogRequested_){ImGui::OpenPopup(ui::tr("Asset dependencies"));assetPanel_->presentation.dependencyDialogRequested_=false;}
    if(ImGui::BeginPopupModal(ui::tr("Content request"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(assetPanel_->presentation.contentOperation_.c_str());
        ui::inputText("Name / destination",assetPanel_->presentation.contentValue_);observeWidget("content.value");
        if(ui::button(ui::tr("Preview impact"))) {
            auto request=assetPanel_->presentation.contentRequest_;request["dryRun"]=true;
            if(assetPanel_->presentation.contentOperation_=="content.create-directory"||assetPanel_->presentation.contentOperation_=="asset.rename")request["name"]=assetPanel_->presentation.contentValue_;
            else request["destination"]=assetPanel_->presentation.contentValue_;
            const auto result=session_->edit(assetPanel_->presentation.contentOperation_,request);
            if(result){assetPanel_->presentation.contentImpact_=result.value;ImGui::CloseCurrentPopup();}
        }observeWidget("content.plan");ImGui::SameLine();
        if(ui::button(ui::tr("Cancel")))ImGui::CloseCurrentPopup();observeWidget("content.cancel-request");
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal(ui::tr("Texture settings"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        widgets_["texture.dialog"]={position.x,position.y,size.x,size.y};
        ImGui::Text(ui::tr("%zu image asset(s)"),assetPanel_->presentation.textureSnapshot_.at("assets").size());
        ImGui::TextWrapped(ui::tr("Choose the fields to apply. Mixed fields keep each asset's value until selected."));
        auto& values=assetPanel_->presentation.textureSnapshot_["values"];
        const std::map<std::string,std::vector<const char*>> choices{
            {"usage",{"albedo","emissive","normal","mask","roughness","linear","packed-normal"}},
            {"colorSpace",{"srgb","linear"}},{"filter",{"linear","nearest"}},
            {"mipFilter",{"linear","nearest"}},
            {"wrapU",{"repeat","clamp","mirror"}},{"wrapV",{"repeat","clamp","mirror"}},
            {"mipPolicy",{"generate","none"}}};
        for(const auto& [field,options]:choices) {
            ImGui::PushID(field.c_str());bool enabled=assetPanel_->presentation.textureFields_.count(field)!=0;
            if(ImGui::Checkbox("##apply",&enabled)){if(enabled)assetPanel_->presentation.textureFields_.insert(field);else assetPanel_->presentation.textureFields_.erase(field);}observeWidget("texture.enable."+field);
            ImGui::SameLine();const auto current=values.at(field).get<std::string>();
            const bool mixed=std::find(assetPanel_->presentation.textureSnapshot_["mixed"].begin(),assetPanel_->presentation.textureSnapshot_["mixed"].end(),nlohmann::json(field))!=assetPanel_->presentation.textureSnapshot_["mixed"].end();
            ImGui::BeginDisabled(!enabled);ImGui::SetNextItemWidth(170*dpi_);
            if(ImGui::BeginCombo(ui::tr(field.c_str()),mixed&&!enabled?ui::tr("Mixed"):ui::tr(current.c_str()))) {
                for(const auto* option:options) {
                    if(ImGui::Selectable(ui::tr(option),current==option))values[field]=option;
                    observeWidget("texture.option."+field+"."+option);
                }ImGui::EndCombo();
            }observeWidget("texture."+field);ImGui::EndDisabled();ImGui::PopID();
        }
        bool anisotropy=assetPanel_->presentation.textureFields_.count("anisotropy")!=0;
        if(ImGui::Checkbox("##anisotropy-apply",&anisotropy)){if(anisotropy)assetPanel_->presentation.textureFields_.insert("anisotropy");else assetPanel_->presentation.textureFields_.erase("anisotropy");}
        observeWidget("texture.enable.anisotropy");ImGui::SameLine();ImGui::BeginDisabled(!anisotropy);
        float amount=values.at("anisotropy").get<float>();if(ImGui::SliderFloat(ui::tr("Anisotropy"),&amount,1,16,"%.0f"))values["anisotropy"]=amount;
        observeWidget("texture.anisotropy");ImGui::EndDisabled();
        bool mipBias=assetPanel_->presentation.textureFields_.count("mipBias")!=0;
        if(ImGui::Checkbox("##mipBias-apply",&mipBias)){if(mipBias)assetPanel_->presentation.textureFields_.insert("mipBias");else assetPanel_->presentation.textureFields_.erase("mipBias");}
        observeWidget("texture.enable.mipBias");ImGui::SameLine();ImGui::BeginDisabled(!mipBias);
        float bias=values.at("mipBias").get<float>();if(ImGui::SliderFloat(ui::tr("Mip bias"),&bias,-16,16,"%.2f"))values["mipBias"]=bias;
        observeWidget("texture.mipBias");ImGui::EndDisabled();
        ui::ButtonOptions apply;apply.disabled=assetPanel_->presentation.textureFields_.empty()||session_->context().dirty()||session_->reimporting();
        if(ui::button(ui::tr("Preview impact"),apply)) {
            auto fields=nlohmann::json::object(),assets=nlohmann::json::array();
            for(const auto& field:assetPanel_->presentation.textureFields_)fields[field]=values.at(field);
            for(const auto& asset:assetPanel_->presentation.textureSnapshot_.at("assets"))assets.push_back(asset.at("id"));
            const auto result=session_->edit("texture.settings",{{"assets",assets},{"values",fields},{"dryRun",true}});
            if(result){assetPanel_->presentation.contentOperation_="texture.settings";assetPanel_->presentation.contentImpact_=result.value;ImGui::CloseCurrentPopup();}
        }observeWidget("texture.plan");ImGui::SameLine();
        if(ui::button(ui::tr("Cancel")))ImGui::CloseCurrentPopup();observeWidget("texture.cancel");
        ImGui::EndPopup();
    }
    if(!assetPanel_->presentation.contentImpact_.is_null()&&!assetPanel_->presentation.contentImpact_.empty()&&!ImGui::IsPopupOpen(ui::tr("Content impact")))ImGui::OpenPopup(ui::tr("Content impact"));
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal(ui::tr("Content impact"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(ui::tr("Apply %s to the following files:"),assetPanel_->presentation.contentOperation_.c_str());
        ImGui::BeginChild(ui::tr("Affected files"),{500*dpi_,180*dpi_});
        for(const auto& path:assetPanel_->presentation.contentImpact_.at("affected"))ImGui::TextUnformatted(path.get<std::string>().c_str());
        ImGui::EndChild();
        ui::ButtonOptions apply;apply.disabled=session_->context().dirty()||session_->reimporting();
        apply.tooltip=ui::tr("Save scene changes before publishing content operations.");
        if(ui::button(ui::tr("Apply"),apply)) {
            const auto result=session_->edit(assetPanel_->presentation.contentOperation_,{{"planId",assetPanel_->presentation.contentImpact_.at("id")}});
            if(result){assetPanel_->presentation.contentImpact_=nullptr;assetPanel_->presentation.selectedAssets_.clear();ImGui::CloseCurrentPopup();}
        }observeWidget("content.apply");ImGui::SameLine();
        if(ui::button(ui::tr("Cancel"))){session_->edit("content.discard",{{"planId",assetPanel_->presentation.contentImpact_.at("id")}});assetPanel_->presentation.contentImpact_=nullptr;ImGui::CloseCurrentPopup();}
        observeWidget("content.cancel-impact");ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({560*dpi_,0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal(ui::tr("Asset dependencies"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        if(assetPanel_->presentation.dependencySnapshot_.contains("path")) {
            ImGui::TextWrapped("%s",assetPanel_->presentation.dependencySnapshot_.at("path").get<std::string>().c_str());
            ImGui::Text(ui::tr("Type: %s"),assetPanel_->presentation.dependencySnapshot_.at("type").get<std::string>().c_str());
            ImGui::Text(ui::tr("Source: %s"),assetPanel_->presentation.dependencySnapshot_.at("ready").get<bool>()?"Available":"Missing");
            if(assetPanel_->presentation.dependencySnapshot_.value("sourceChanged",false)||assetPanel_->presentation.dependencySnapshot_.value("activeChanged",false))ImGui::TextWrapped(ui::tr("Source or active content changed. Validate with Reimport or Reload Assets."));
            for(const auto* field:{"dependencies","referencedBy"})if(ImGui::CollapsingHeader(field))
                for(const auto& row:assetPanel_->presentation.dependencySnapshot_.at(field))ImGui::TextWrapped("%s",row.at("path").get<std::string>().c_str());
            for(const auto* field:{"id","fingerprint","source","settings","generation","importGeneration","diagnostic"})if(assetPanel_->presentation.dependencySnapshot_.contains(field)&&ImGui::CollapsingHeader(field))
                ImGui::TextWrapped("%s",assetPanel_->presentation.dependencySnapshot_.at(field).dump(2).c_str());
        }else {
            ImGui::TextWrapped(ui::tr("%s dependencies:"),assetPanel_->presentation.dependencySnapshot_.at("direction").get<std::string>().c_str());
            for(const auto& id:assetPanel_->presentation.dependencySnapshot_.at("assets")) {
                const auto& record=session_->context().assets().records().at(id.get<std::string>());
                ImGui::TextWrapped("%s",record.virtualPath.c_str());
            }
        }
        if(ui::button(ui::tr("Close")))ImGui::CloseCurrentPopup();observeWidget("content.close-dependencies");ImGui::EndPopup();
    }
}
}
#endif
