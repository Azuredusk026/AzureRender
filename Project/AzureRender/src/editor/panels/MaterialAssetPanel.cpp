#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawMaterialAssetPanel(){
    if(materialDialogRequested_){ImGui::OpenPopup("Material asset");materialDialogRequested_=false;}
    const auto work=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(620*dpi_,work.x-24*dpi_),std::min(760*dpi_,work.y-24*dpi_)},ImGuiCond_Always);
    if(!ImGui::BeginPopupModal("Material asset",nullptr,ImGuiWindowFlags_NoResize))return;
    const auto label=[&](const std::string& id){const auto& records=session_->context().assets().records();const auto found=records.find(id);return found==records.end()?id:found->second.virtualPath;};
    const auto refreshed=session_->edit("material.describe",{{"asset",materialAsset_}});
    if(refreshed)materialSnapshot_=refreshed.value;
    ImGui::BeginChild("Material parameters",{0,-42*dpi_},false);
    if(materialSnapshot_.contains("document")){
        ImGui::TextUnformatted(materialSnapshot_.at("document").at("typeId").get<std::string>().c_str());
        if(session_->developerServices().image){const auto preview=session_->edit("preview.asset",{{"asset",materialAsset_}});if(preview)if(const auto texture=previewTexture(preview.value.at("handle").get<std::uint64_t>()))ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{160*dpi_,160*dpi_});}
        for(const auto& field:materialSnapshot_.at("type").items()){
            const auto& specification=field.value();const auto value=materialSnapshot_.at("document").at("parameters").at(field.key());const auto kind=specification.at("type").get<std::string>();
            nlohmann::json next=value;bool changed=false;
            if(kind=="color"){auto color=value.get<std::array<float,4>>();changed=ImGui::ColorEdit4(field.key().c_str(),color.data());next=color;}
            else if(kind=="boolean"){auto boolean=value.get<bool>();changed=ImGui::Checkbox(field.key().c_str(),&boolean);next=boolean;}
            else {float number=value.get<float>();changed=ImGui::SliderFloat(field.key().c_str(),&number,specification.at("min").get<float>(),specification.at("max").get<float>());next=number;}
            observeWidget("material.field."+field.key());
            if(changed)session_->edit("material.field",{{"asset",materialAsset_},{"field",field.key()},{"value",next}});
        }
        for(const auto& slot:materialSnapshot_.at("textures").items()){
            const auto& document=materialSnapshot_.at("document");const auto current=document.at("textures").contains(slot.key())?document.at("textures").at(slot.key()).at("asset").get<std::string>():std::string();
            if(ImGui::BeginCombo((slot.key()+" texture").c_str(),current.empty()?"None":label(current).c_str())){
                if(ImGui::Selectable("None",current.empty()))session_->edit("material.field",{{"asset",materialAsset_},{"field","textures."+slot.key()},{"value",nullptr}});
                for(const auto& [id,record]:session_->context().assets().records())if(assetTypeRegistry().matches(record.path,{"image"})){
                    if(ImGui::Selectable(record.virtualPath.c_str(),current==id))session_->edit("material.field",{{"asset",materialAsset_},{"field","textures."+slot.key()},{"value",{{"asset",id},{"usage",slot.value()}}}});
                    observeWidget("material.texture."+slot.key()+"."+id);
                }ImGui::EndCombo();
            }observeWidget("material.texture."+slot.key());
        }
        ImGui::TextWrapped("Parameters publish to this asset. Scene slot assignments use scene history.");
        if(ui::button("Undo material",{ui::ButtonVariant::Default,false,!materialSnapshot_.at("canUndo").get<bool>()}))session_->edit("material.undo",{{"asset",materialAsset_}});observeWidget("material.undo");ImGui::SameLine();
        if(ui::button("Redo material",{ui::ButtonVariant::Default,false,!materialSnapshot_.at("canRedo").get<bool>()}))session_->edit("material.redo",{{"asset",materialAsset_}});observeWidget("material.redo");
        for(const auto& error:session_->feedback().report())if(error.at("source").get<std::string>().rfind("material.",0)==0)ImGui::TextWrapped("%s",error.at("message").get<std::string>().c_str());
    }
    ImGui::EndChild();
    if(ui::button("Close material")){materialAsset_.clear();materialSnapshot_=nullptr;ImGui::CloseCurrentPopup();}observeWidget("material.close");ImGui::EndPopup();
}
void ImGuiEditorLayer::drawMaterialSlots(const SceneNode& node){
    if(node.resourceId.empty()||!session_->context().isProject())return;
    const bool open=ImGui::CollapsingHeader("Materials");observeWidget("material.slots-header");if(!open)return;
    const auto result=session_->edit("material.slots",{{"node",node.id}});if(!result)return;
    const auto label=[&](const std::string& id){const auto& records=session_->context().assets().records();const auto found=records.find(id);return found==records.end()?id:found->second.virtualPath;};
    for(const auto& slot:result.value.at("slots")){
        const auto id=slot.at("id").get<std::string>();ImGui::PushID(id.c_str());const auto assigned=slot.at("override").is_null()?std::string():slot.at("override").get<std::string>();
        if(ImGui::BeginCombo(slot.at("name").get<std::string>().c_str(),assigned.empty()?"Source material":label(assigned).c_str())){
            for(const auto& [uuid,record]:session_->context().assets().records())if(assetTypeRegistry().matches(record.path,{"material"})){
                if(ImGui::Selectable(record.virtualPath.c_str(),uuid==assigned))session_->edit("material.assign",{{"nodes",session_->selection().selected()},{"slotId",id},{"material",uuid}});
                observeWidget("material.assign."+id+"."+uuid);
            }ImGui::EndCombo();
        }observeWidget("material.slot."+id);
        if(ImGui::BeginDragDropTarget()){if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE"))session_->edit("material.assign",{{"nodes",session_->selection().selected()},{"slotId",id},{"material",std::string(static_cast<const char*>(payload->Data))}});ImGui::EndDragDropTarget();}
        if(ui::button("Reset slot",{ui::ButtonVariant::Default,false,assigned.empty()}))session_->edit("material.reset-slot",{{"nodes",session_->selection().selected()},{"slotId",id}});observeWidget("material.reset."+id);
        if(!slot.at("stable").get<bool>())ImGui::TextWrapped("Anonymous or duplicate source names use an index slot. Set azureSlotId in the model for stable reimport mapping.");
        ImGui::PopID();
    }
    for(const auto& diagnostic:result.value.at("diagnostics")){
        const auto id=diagnostic.at("slot").get<std::string>();ImGui::TextWrapped("Missing source slot: %s",id.c_str());
        if(ui::button(("Clear missing slot##"+id).c_str()))session_->edit("material.reset-slot",{{"nodes",session_->selection().selected()},{"slotId",id}});
    }
}
}
#endif
