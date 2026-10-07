#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/content/AssetCatalog.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "editor/ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height) {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawAssetBrowserPanel(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin("Content Browser###assets",workspace_.open("assets"))){ImGui::End();return;}
    const bool compact=ImGui::GetContentRegionAvail().y<150*dpi_;
    const bool wide=ImGui::GetContentRegionAvail().x>360*dpi_;
    assetFilter_.Draw("##Search assets",wide?ImGui::GetContentRegionAvail().x-(compact?280:230)*dpi_:-1);observeWidget("assets.search");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Search asset names and paths");if(wide)ImGui::SameLine();
    ImGui::SetNextItemWidth(140*dpi_);
    const auto& types=assetTypeRegistry().types();
    const auto current=types.find(assetTypeId_);
    if(ImGui::BeginCombo("##Type",current==types.end()?"All":current->second.label.c_str(),ImGuiComboFlags_HeightLarge)) {
        if(ImGui::Selectable("All",assetTypeId_.empty()))assetTypeId_.clear();observeWidget("type.All");
        for(const auto& [id,type]:types){if(ImGui::Selectable(type.label.c_str(),assetTypeId_==id))assetTypeId_=id;observeWidget("type."+type.label);}
        ImGui::EndCombo();
    }observeWidget("assets.type");ImGui::SameLine();ImGui::Checkbox("Grid",&assetGrid_);observeWidget("assets.grid");
    const auto resources=session_->edit("assets.catalog",{{"type",assetTypeId_}}).value;
    const auto directoryFilter=[&]{
        std::set<std::string> directories;for(const auto& row:resources)directories.insert(std::filesystem::u8path(row.at("source").get<std::string>()).parent_path().u8string());
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##Directory",assetDirectory_.empty()?"All directories":assetDirectory_.c_str())){
            if(ImGui::Selectable("All directories",assetDirectory_.empty()))assetDirectory_.clear();
            for(const auto& directory:directories)if(ImGui::Selectable(directory.c_str(),directory==assetDirectory_))assetDirectory_=directory;
            ImGui::EndCombo();
        }
    };
    if(compact)ImGui::SameLine();
    if(ui::button(compact?"+":"Import / Create"))ImGui::OpenPopup("Content operations");observeWidget("assets.import-create");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Import, create and browse content");
    ImGui::SetNextWindowSize({std::min(640*dpi_,ImGui::GetIO().DisplaySize.x-32*dpi_),0});
    if(ImGui::BeginPopup("Content operations")) {
        if(compact){directoryFilter();if(ui::button("Reload Assets"))static_cast<void>(session_->execute(EditorCommand::ReloadAssets));ImGui::Separator();}
        drawPathInput("glTF / GLB source",importPath_,"import",false,{".gltf",".glb"});
        ui::ButtonOptions import;import.disabled=view.importing()||session_->playing()||session_->building()||importPath_.empty();
        import.tooltip="Select a glTF or GLB file. Import continues when this panel is closed.";
        if(ui::button("Import",import)){const auto result=session_->edit("asset.import-start",{{"path",importPath_}});if(result)ImGui::CloseCurrentPopup();}observeWidget("asset.import");
        if(view.importing()){ImGui::ProgressBar(view.importProgress());if(ui::button("Cancel Import"))session_->edit("asset.import-cancel");observeWidget("asset.import-cancel");}
        ui::inputText("Prefab instance",prefabInstance_);
        ImGui::BeginDisabled(session_->playing()||session_->building());
        if(ui::button("Create empty node"))session_->edit("node.create",{{"id",prefabInstance_}});
        if(ImGui::BeginCombo("Place Prefab","Choose asset")){
            for(const auto& row:session_->edit("assets.catalog",{{"type","prefab"}}).value)if(ImGui::Selectable(row.at("path").get<std::string>().c_str()))
                session_->edit("asset.place",{{"asset",row.at("id")},{"origin",{0,5,0}},{"direction",{0,-1,0}}});
            ImGui::EndCombo();
        }ImGui::EndDisabled();
        const auto& summary=view.importSummary();if(summary.contains("vertices"))ImGui::TextWrapped("Imported: %zu vertices, %zu joints, %zu materials",summary.at("vertices").get<std::size_t>(),summary.at("joints").get<std::size_t>(),summary.at("materials").get<std::size_t>());
        ImGui::EndPopup();
    }
    if(!compact){ImGui::SameLine();if(ui::button("Reload Assets"))static_cast<void>(session_->execute(EditorCommand::ReloadAssets));ImGui::SameLine();directoryFilter();}
    visibleAssets_=nlohmann::json::array();ImGui::BeginChild("asset-list",{0,0});std::size_t requestedPreviews=0;
    if(ImGui::BeginTable("assets",assetGrid_?std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/(180*dpi_))):4,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY)){
        if(!assetGrid_){for(const auto* title:{"Asset","Type","State","Users"})ImGui::TableSetupColumn(title);if(!compact)ImGui::TableHeadersRow();}
        for(const auto& row:resources){
            const auto id=row.at("id").get<std::string>(),label=row.at("path").get<std::string>(),type=row.at("type").get<std::string>();
            const auto path=std::filesystem::u8path(row.at("source").get<std::string>());const bool ready=row.at("ready").get<bool>();
            if(!assetFilter_.PassFilter(label.c_str())||(!assetDirectory_.empty()&&path.parent_path().u8string()!=assetDirectory_))continue;
            visibleAssets_.push_back(row);ImGui::PushID(id.c_str());
            if(assetGrid_)ImGui::TableNextColumn();else{ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
            if(assetGrid_&&type=="model"&&requestedPreviews<2&&ImGui::IsRectVisible({96*dpi_,96*dpi_})&&session_->developerServices().image){
                ++requestedPreviews;const auto result=session_->edit("preview.asset",{{"asset",id}});
                if(result&&result.value.contains("handle"))if(const auto texture=previewTexture(result.value.at("handle").get<std::uint64_t>()))ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{96*dpi_,96*dpi_});
            }
            const auto registered=types.find(type);const bool placeable=registered!=types.end()&&registered->second.placeable;
            if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick,{ImGui::GetContentRegionAvail().x,assetGrid_?44*dpi_:0})&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)&&placeable&&!session_->playing()&&!session_->building())session_->edit("asset.place",{{"asset",id},{"origin",{0,5,0}},{"direction",{0,-1,0}}});
            observeWidget("asset."+id);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\n%s",label.c_str(),path.u8string().c_str(),ready?"Ready":"Missing source");
            if(placeable&&ready&&ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("AZURE_RESOURCE",id.c_str(),id.size()+1);ImGui::TextUnformatted(label.c_str());ImGui::EndDragDropSource();}
            if(!assetGrid_){ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(type.c_str());ImGui::TableSetColumnIndex(2);ImGui::TextUnformatted(ready?"Ready":"Missing");ImGui::TableSetColumnIndex(3);ImGui::Text("%zu",row.at("users").get<std::size_t>());}
            ImGui::PopID();
        }ImGui::EndTable();
    }
    if(visibleAssets_.empty())ImGui::TextWrapped("No assets match the directory, search and type filters.");
    ImGui::EndChild();ImGui::End();
}


}
#endif
