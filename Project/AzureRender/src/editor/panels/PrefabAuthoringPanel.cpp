#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawPrefabAuthoringPanel(PanelContext& panel){
    if(!ImGui::Begin("预制体制作###prefabs",workspace_.open("prefabs"))){ImGui::End();return;}
    if(!panel.view().isProject()){ImGui::TextUnformatted("Open a project to author prefabs.");ImGui::End();return;}
    ImGui::BeginDisabled(session_->playing()||session_->building());
    ImGui::SetNextItemWidth(-1);ui::inputText("Source output",prefabOutputPath_);observeWidget("prefab.output");
    ImGui::BeginDisabled(session_->selection().selected().empty());
    if(ui::button("Create from selection"))session_->edit("prefab.create",{{"nodes",session_->selection().selected()},{"path",prefabOutputPath_}});observeWidget("prefab.create");ImGui::EndDisabled();
    ImGui::Separator();
    const auto instances=panel.view().levelDocument().value("prefabs",nlohmann::json::array());
    if(const auto* selected=panel.view().selectedNode();selected&&!selected->instanceOf.empty())prefabOverrideInstance_=selected->id.substr(0,selected->id.find(':'));
    if(ImGui::BeginCombo("Instance",prefabOverrideInstance_.c_str())){
        for(const auto& instance:instances){const auto id=instance.at("instance").get<std::string>();if(ImGui::Selectable(id.c_str(),id==prefabOverrideInstance_))prefabOverrideInstance_=id;observeWidget("prefab.instance."+id);}ImGui::EndCombo();
    }observeWidget("prefab.instance");
    bool present=false;for(const auto& instance:instances)if(instance.at("instance")==prefabOverrideInstance_)present=true;
    if(present){
        const auto result=session_->edit("prefab.overrides",{{"instance",prefabOverrideInstance_}});
        if(result){const auto& description=result.value;const auto scope=panel.view().documentId()+":"+prefabOverrideInstance_+":"+description.at("baseFingerprint").get<std::string>();
            if(scope!=prefabFieldScope_){prefabSelectedFields_.clear();prefabFieldScope_=scope;}
            const bool changed=description.at("sourceChanged").get<bool>();
            if(changed)ImGui::TextWrapped("Source changed. Save your scene, then reload it to resolve the source version before editing overrides.");
            auto fields=nlohmann::json::array();std::set<std::string> live;
            for(const auto& field:description.at("fields")){
                const auto node=field.at("node").get<std::string>(),path=field.at("path").get<std::string>(),key=node+"\n"+path;live.insert(key);bool selected=prefabSelectedFields_.count(key)>0;
                const auto label=node+" : "+path;ImGui::PushID(key.c_str());
                if(ImGui::Checkbox(label.c_str(),&selected)){if(selected)prefabSelectedFields_.insert(key);else prefabSelectedFields_.erase(key);}observeWidget("prefab.field."+node+path);ImGui::PopID();
                if(selected)fields.push_back({{"node",node},{"path",path}});
            }
            for(auto it=prefabSelectedFields_.begin();it!=prefabSelectedFields_.end();)if(!live.count(*it))it=prefabSelectedFields_.erase(it);else ++it;
            if(description.at("fields").empty())ImGui::TextUnformatted("This instance inherits all source fields.");
            ImGui::TextWrapped("Apply updates the source and every open scene using it. Independent overrides stay in their scenes.");
            ImGui::BeginDisabled(changed||fields.empty());
            if(ui::button("Revert selected fields"))session_->edit("prefab.revert",{{"instance",prefabOverrideInstance_},{"fields",fields}});observeWidget("prefab.revert");ImGui::SameLine();
            if(ui::button("Apply selected fields to source"))session_->edit("prefab.apply",{{"instance",prefabOverrideInstance_},{"fields",fields},{"baseFingerprint",description.at("baseFingerprint")}});observeWidget("prefab.apply");ImGui::EndDisabled();
        }
    }else ImGui::TextUnformatted("Place a prefab from the Asset Browser to inspect its overrides.");
    if(ui::button("Undo source application"))session_->edit("prefab.undo-apply");observeWidget("prefab.undo-apply");ImGui::SameLine();
    if(ui::button("Redo source application"))session_->edit("prefab.redo-apply");observeWidget("prefab.redo-apply");
    ImGui::TextWrapped("Scene overrides use normal Undo/Redo. Source undo requires the affected scenes and source to retain their application versions.");
    ImGui::EndDisabled();ImGui::End();
}
}
#endif
