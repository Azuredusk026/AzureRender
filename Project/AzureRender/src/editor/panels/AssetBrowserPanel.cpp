#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
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
    assetFilter_.Draw("Search assets",250*dpi_);observeWidget("assets.search");ImGui::SameLine();
    ImGui::SetNextItemWidth(120*dpi_);
    const char* types[]={"All","Models","Scripts","Prefabs"};
    if(ImGui::BeginCombo("Type",types[assetType_])){
        for(int type=0;type<4;++type){if(ImGui::Selectable(types[type],assetType_==type))assetType_=type;observeWidget(std::string("type.")+types[type]);}
        ImGui::EndCombo();
    }observeWidget("assets.type");ImGui::SameLine();
    ImGui::Checkbox("Grid",&assetGrid_);observeWidget("assets.grid");
    if(view.isProject() && ImGui::CollapsingHeader("Import / Create")){
        static std::array<char,1024> source{};ImGui::InputText("glTF / GLB path",source.data(),source.size());
        ImGui::BeginDisabled(view.importing());if(ui::button("Import"))try{session_->edit("asset.import-start",{{"path",source.data()}});}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}ImGui::EndDisabled();
        if(view.importing()){ImGui::ProgressBar(view.importProgress());if(ui::button("Cancel Import"))session_->edit("asset.import-cancel");}
        if(view.importing())try{if(auto imported=session_->edit("asset.import-poll").value;!imported.is_null())session_->log("Import ready: "+imported.get<std::string>());}catch(const std::exception& error){session_->log(std::string("Import: ")+error.what());}
        const auto& summary=view.importSummary();
        if(summary.contains("vertices")){
            ImGui::Text("Admission: %zu vertices, %zu joints, %zu materials",summary.at("vertices").get<std::size_t>(),summary.at("joints").get<std::size_t>(),summary.at("materials").get<std::size_t>());
            for(const auto& clip:summary.at("clips"))ImGui::Text("Clip %u: %s (%.3f s)",clip.at("index").get<unsigned>(),clip.at("name").get<std::string>().c_str(),clip.at("duration").get<double>());
        }
        static std::array<char,128> instance{};
        ImGui::InputText("Prefab instance",instance.data(),instance.size());
        ImGui::BeginDisabled(session_->playing() || session_->building());
        if(ui::button("Create empty node"))try{session_->edit("node.create",{{"id",instance.data()}});}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}
        if(ImGui::BeginCombo("Place Prefab","Choose asset")){
            for(const auto& [id,record]:view.assets().records())if(record.path.extension()==".azureprefab" && ImGui::Selectable(record.virtualPath.c_str()))
                try{session_->edit("prefab.place",{{"asset",id},{"instance",instance.data()}});}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    }

    if (ui::button("Reload Assets")) {
        static_cast<void>(session_->execute(EditorCommand::ReloadAssets));
    }
    auto resources=view.resourceStatuses();
    std::map<std::string,std::string> labels;
    for(const auto& resource:resources)labels[resource.id]=resource.path.filename().u8string();
    if(view.isProject())for(const auto& [id,record]:view.assets().records()) {
        const auto found=std::find_if(resources.begin(),resources.end(),[&](const auto& resource){return resource.path==record.path;});
        if(found==resources.end()){
            EditorContext::ResourceStatus status;status.id=id;status.path=record.path;status.exists=std::filesystem::is_regular_file(record.path);
            resources.push_back(status);labels[id]=record.virtualPath;
        }else labels[found->id]=record.virtualPath;
    }
    std::set<std::string> directories;
    for(const auto& resource:resources)directories.insert(resource.path.parent_path().u8string());
    ImGui::SameLine();ImGui::SetNextItemWidth(250*dpi_);
    if(ImGui::BeginCombo("Directory",assetDirectory_.empty()?"All directories":std::filesystem::u8path(assetDirectory_).filename().u8string().c_str())){
        if(ImGui::Selectable("All directories",assetDirectory_.empty()))assetDirectory_.clear();
        for(const auto& directory:directories)if(ImGui::Selectable(directory.c_str(),directory==assetDirectory_))assetDirectory_=directory;
        ImGui::EndCombo();
    }
    visibleAssets_=nlohmann::json::array();
    auto place=[&](const EditorContext::ResourceStatus& resource){
        if(session_->playing() || session_->building())return;
        try{
            const auto extension=resource.path.extension();
            if(extension==".azureprefab")session_->edit("prefab.place",{{"asset",resource.id},{"instance","prefab-"+std::to_string(view.scene().nodes.size())}});
            else if(extension==".gltf" || extension==".glb"){
                auto id=resource.id;
                if(std::none_of(view.scene().resources.begin(),view.scene().resources.end(),[&](const auto& entry){return entry.id==id;})){auto imported=session_->edit("asset.import",{{"path",resource.path.string()}});if(!imported)throw std::runtime_error(session_->lastError());id=imported.value.get<std::string>();};
                session_->edit("node.place",{{"resource",id}});
            }
        }catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}
    };
    ImGui::BeginChild("asset-list",{0,0});
    std::size_t requestedPreviews=0;
    if(ImGui::BeginTable("assets",assetGrid_?std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/(180*dpi_))):4,
        ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY)){
        if(!assetGrid_){for(const auto* title:{"Asset","Type","State","Users"})ImGui::TableSetupColumn(title);ImGui::TableHeadersRow();}
        for(const auto& resource:resources){
            const auto extension=resource.path.extension().u8string();const auto label=labels.at(resource.id);
            if(!assetFilter_.PassFilter(label.c_str()))continue;
            if(!assetDirectory_.empty() && resource.path.parent_path().u8string()!=assetDirectory_)continue;
            if((assetType_==1&&extension!=".gltf"&&extension!=".glb")||(assetType_==2&&extension!=".lua")||(assetType_==3&&extension!=".azureprefab"))continue;
            visibleAssets_.push_back({{"id",resource.id},{"path",label},{"type",extension},{"ready",resource.exists}});
            ImGui::PushID(resource.id.c_str());
            if(assetGrid_)ImGui::TableNextColumn();else{ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
            if(assetGrid_ && ImGui::IsRectVisible({96*dpi_,96*dpi_}) && requestedPreviews<2 && (extension==".gltf" || extension==".glb") && session_->developerServices().image){
                ++requestedPreviews;
                const auto result=session_->edit("preview.asset",{{"asset",resource.id}});
                if(result && result.value.contains("handle"))try{
                    if(const auto texture=previewTexture(result.value.at("handle").get<std::uint64_t>()))
                        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{96*dpi_,96*dpi_});
                    else ImGui::TextDisabled("Preview pending");
                }catch(const std::exception& error){session_->log(error.what());}
            }
            if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick,assetGrid_?ImVec2{0,44*dpi_}:ImVec2{0,0}) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))place(resource);
            observeWidget("asset."+resource.id);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",label.c_str(),resource.exists?"Ready":"ERROR: Missing source");
            const bool model=extension==".gltf" || extension==".glb";
            const bool sceneResource=std::any_of(view.scene().resources.begin(),view.scene().resources.end(),[&](const auto& entry){return entry.id==resource.id;});
            if(model && sceneResource && ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("AZURE_RESOURCE",resource.id.c_str(),resource.id.size()+1);ImGui::TextUnformatted(label.c_str());ImGui::EndDragDropSource();}
            if(!assetGrid_){ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(extension.c_str());ImGui::TableSetColumnIndex(2);ImGui::TextUnformatted(resource.exists?"Ready":"ERROR: Missing");ImGui::TableSetColumnIndex(3);ImGui::Text("%zu",resource.dependentNodeCount);}
            ImGui::PopID();
        }ImGui::EndTable();
    }
    if(visibleAssets_.empty())ImGui::TextDisabled("No assets match the directory, search and type filters.");
    ImGui::EndChild();ImGui::End();
}


}
#endif
