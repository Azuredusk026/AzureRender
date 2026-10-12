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
    const auto metrics=ui::UiMetrics::fromScale(dpi_);
    const auto reveal=session_->consumeAssetReveal();
    if(!reveal.empty()) {assetFilter_.Clear();assetTypeId_.clear();assetDirectory_.clear();assetMount_.clear();selectedAssets_={reveal};}
    const bool compact=ImGui::GetContentRegionAvail().y<metrics.compactPanelHeight;
    const bool renderGrid=assetGrid_&&!compact;
    const bool wide=ImGui::GetContentRegionAvail().x>metrics.widePanelWidth;
    const auto gridWidth=ImGui::GetFrameHeight()+ImGui::CalcTextSize("网格").x+metrics.spacing;
    const auto searchWidth=wide?ImGui::GetContentRegionAvail().x-metrics.filterWidth-gridWidth-3*metrics.spacing-(compact?ui::buttonWidth("+")+metrics.spacing:0):-1;
    ui::searchField("##Search assets","搜索名称或路径",assetFilter_,searchWidth);observeWidget("assets.search");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("按名称或路径筛选资源");
    ui::nextToolbarItem(metrics.filterWidth);ImGui::SetNextItemWidth(metrics.filterWidth);
    const auto& types=assetTypeRegistry().types();
    const auto current=types.find(assetTypeId_);
    if(ImGui::BeginCombo("##Type",current==types.end()?"所有类型":current->second.label.c_str(),ImGuiComboFlags_HeightLarge)) {
        if(ImGui::Selectable("所有类型",assetTypeId_.empty()))assetTypeId_.clear();observeWidget("type.All");
        for(const auto& [id,type]:types){if(ImGui::Selectable(type.label.c_str(),assetTypeId_==id))assetTypeId_=id;observeWidget("type."+type.label);}
        ImGui::EndCombo();
    }observeWidget("assets.type");ui::nextToolbarItem(gridWidth);ImGui::Checkbox("网格",&assetGrid_);observeWidget("assets.grid");
    auto& directories=session_->contentDirectories();
    const auto navigate=[&](const std::string& mount,const std::string& path){
        if(assetMount_==mount&&assetDirectory_==path)return;
        assetNavigationDiagnostic_.clear();
        assetMount_=mount;assetDirectory_=path;
        assetNavigation_.resize(assetNavigationIndex_+1);assetNavigation_.push_back({mount,path});++assetNavigationIndex_;
    };
    std::shared_ptr<const ContentDirectorySnapshot> snapshot;
    try{snapshot=directories.snapshot(session_->context(),assetMount_,assetDirectory_,assetTypeId_,assetFilter_.InputBuf);}
    catch(const std::exception& error){assetNavigationDiagnostic_=error.what();assetMount_.clear();assetDirectory_.clear();snapshot=directories.snapshot(session_->context(),{}, {},assetTypeId_,assetFilter_.InputBuf);}
    const auto& resources=snapshot->assets;
    contentView_={{"matchingAssets",resources.size()},{"search",assetFilter_.InputBuf},{"type",assetTypeId_},{"grid",renderGrid},{"compact",compact},{"selectedCount",selectedAssets_.size()},{"diagnostic",assetNavigationDiagnostic_}};
    const auto directoryFilter=[&]{
        if(ui::button("后退",{ui::ButtonVariant::Default,false,assetNavigationIndex_==0})){const auto& entry=assetNavigation_[--assetNavigationIndex_];assetMount_=entry.first;assetDirectory_=entry.second;}observeWidget("assets.back");ui::nextToolbarItem(ui::buttonWidth("前进"));
        if(ui::button("前进",{ui::ButtonVariant::Default,false,assetNavigationIndex_+1>=assetNavigation_.size()})){const auto& entry=assetNavigation_[++assetNavigationIndex_];assetMount_=entry.first;assetDirectory_=entry.second;}observeWidget("assets.forward");ui::nextToolbarItem(ui::buttonWidth("全部"));
        if(ui::button("全部"))navigate({},{});observeWidget("assets.all");
        if(!assetMount_.empty()){
            ui::nextToolbarItem(ui::buttonWidth(assetMount_.c_str()));if(ui::button(assetMount_.c_str()))navigate(assetMount_,{});observeWidget("assets.mount");
            std::string accumulated;for(const auto& part:std::filesystem::u8path(assetDirectory_)){
                if(!accumulated.empty())accumulated+='/';accumulated+=part.u8string();
                ui::nextToolbarItem(ui::buttonWidth(part.u8string().c_str()));ImGui::PushID(accumulated.c_str());
                if(ui::button(part.u8string().c_str()))navigate(assetMount_,accumulated);observeWidget("assets.breadcrumb."+accumulated);ImGui::PopID();
            }
        }
    };
    ui::nextToolbarItem(ui::buttonWidth(compact?"+":"导入 / 创建"));
    if(ui::button(compact?"+":"导入 / 创建"))ImGui::OpenPopup("Content operations");observeWidget("assets.import-create");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Import, create and browse content");
    ImGui::SetNextWindowSize({std::min(640*dpi_,ImGui::GetIO().DisplaySize.x-32*dpi_),0});
    if(ImGui::BeginPopup("Content operations")) {
        if(compact){
            directoryFilter();ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("Directory",assetMount_.empty()?"All directories":(assetMount_+":/"+assetDirectory_).c_str())){
                if(ImGui::Selectable("All directories",assetMount_.empty()))navigate({},{});
                for(const auto& directory:directories.tree())if(ImGui::Selectable(directory.at("reference").get<std::string>().c_str()))navigate(directory.at("mount").get<std::string>(),directory.at("path").get<std::string>());
                ImGui::EndCombo();
            }observeWidget("assets.directory");
            if(ui::button("Reload Assets"))static_cast<void>(session_->execute(EditorCommand::ReloadAssets));ImGui::Separator();
        }
        std::vector<std::string> importExtensions;
        for(const auto& [id,format]:assetFormatRegistry().formats()){(void)id;if(format.canRead)importExtensions.insert(importExtensions.end(),format.extensions.begin(),format.extensions.end());}
        drawPathInput("Asset source",importPath_,"import",false,importExtensions);
        ui::ButtonOptions import;import.disabled=view.importing()||session_->playing()||session_->building()||importPath_.empty();
        import.tooltip="Select a supported model, image or audio file. Import continues when this panel is closed.";
        if(ui::button("Import",import)){const auto result=session_->edit("asset.import-start",{{"path",importPath_}});if(result)ImGui::CloseCurrentPopup();}observeWidget("asset.import");
        if(view.importing()){ImGui::ProgressBar(view.importProgress());if(ui::button("Cancel Import"))session_->edit("asset.import-cancel");observeWidget("asset.import-cancel");}
        ui::ButtonOptions exportOptions;exportOptions.disabled=selectedAssets_.size()!=1||session_->context().dirty()||view.importing()||session_->playing()||session_->building()||session_->reimporting();
        if(ui::button("Export selected asset",exportOptions)){exportRequest_={{"asset",*selectedAssets_.begin()}};exportDialogRequested_=true;ImGui::CloseCurrentPopup();}observeWidget("assets.export");
        ImGui::BeginDisabled(!view.isProject()||session_->playing()||session_->building()||session_->reimporting());
        if(ui::button("Create directory")){contentOperation_="content.create-directory";contentRequest_={{"parent",(assetMount_.empty()?"assets":assetMount_)+":/"+assetDirectory_}};contentValue_="New Folder";contentDialogRequested_=true;ImGui::CloseCurrentPopup();}
        observeWidget("content.create-directory");
        if(ImGui::BeginMenu("Create material")){
            ui::inputText("Name",materialName_);observeWidget("material.create-name");
            for(const auto& type:MaterialTypeRegistry::builtins().describe()){
                const auto id=type.at("id").get<std::string>();
                if(ImGui::MenuItem(type.at("label").get<std::string>().c_str())){
                    const auto destination=(assetMount_.empty()?"assets:/":assetMount_+":/")+(assetDirectory_.empty()?"":assetDirectory_+"/")+materialName_;
                    const auto created=session_->edit("material.create",{{"type",id},{"destination",destination}});
                    if(created){materialAsset_=created.value.at("asset").get<std::string>();const auto opened=session_->edit("material.describe",{{"asset",materialAsset_}});if(opened){materialSnapshot_=opened.value;materialDialogRequested_=true;}}
                }observeWidget("material.create."+id);
            }ImGui::EndMenu();
        }observeWidget("assets.create-material");
        if(ImGui::BeginMenu("Create object")){
            for(const auto& preset:session_->nodePresets().describe()){
                const auto id=preset.at("id").get<std::string>();if(ImGui::MenuItem(preset.at("label").get<std::string>().c_str()))session_->edit("node.create-preset",{{"preset",id}});observeWidget("preset."+id);
            }ImGui::EndMenu();
        }observeWidget("assets.create-object");
        if(ImGui::BeginMenu("Generate primitive")){
            if(ImGui::BeginCombo("Shape",primitiveKind_.c_str())){for(const auto* kind:{"cube","plane","sphere","cylinder"})if(ImGui::Selectable(kind,primitiveKind_==kind))primitiveKind_=kind;ImGui::EndCombo();}observeWidget("primitive.kind");
            ImGui::InputFloat3("Dimensions",primitiveSize_.data());observeWidget("primitive.size");
            ImGui::InputInt("Segments",&primitiveSegments_);observeWidget("primitive.segments");
            ui::inputText("File name",primitiveName_);observeWidget("primitive.name");
            if(ui::button("Generate asset")){
                const auto parent=(assetMount_.empty()?"assets":assetMount_)+":/"+assetDirectory_;
                const auto output=parent+(parent.back()=='/'?"":"/")+primitiveName_;
                const auto result=session_->edit("asset.generate",{{"generator","azure.primitive"},{"output",output},{"parameters",{{"kind",primitiveKind_},{"size",primitiveSize_},{"segments",primitiveSegments_}}},{"license","Azure primitive generator v1"},{"replace",false}});
                if(result)selectedAssets_={result.value.get<std::string>()};
            }observeWidget("primitive.generate");ImGui::TextWrapped("Sphere and cylinder segments use a multiple of four.");ImGui::EndMenu();
        }observeWidget("assets.generate-primitive");
        if(ui::button("Undo content transaction"))session_->edit("content.undo");observeWidget("content.undo");ImGui::EndDisabled();
        ImGui::BeginDisabled(!view.isProject()||session_->playing()||session_->building()||session_->reimporting());
        if(ui::button("Redo content transaction"))session_->edit("content.redo");observeWidget("content.redo");ImGui::EndDisabled();
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
    if(!compact){ui::nextToolbarItem(ui::buttonWidth("刷新资源"));if(ui::button("刷新资源"))static_cast<void>(session_->execute(EditorCommand::ReloadAssets));observeWidget("assets.reload");directoryFilter();}
    if(!assetNavigationDiagnostic_.empty())ui::resultMessage(assetNavigationDiagnostic_,true);
    if(!compact){ui::statusText((std::to_string(resources.size())+" 项资源 · "+std::to_string(selectedAssets_.size())+" 项已选").c_str(),ui::StatusKind::Neutral);}
    if(renderGrid){int size=session_->settings().get("editor.thumbnailSize").get<int>();ImGui::SetNextItemWidth(120*dpi_);if(ImGui::SliderInt("Thumbnail size",&size,64,192))session_->edit("settings.set",{{"name","editor.thumbnailSize"},{"value",size},{"source","user"}});observeWidget("assets.thumbnail-size");}
    const auto thumbnailSize=session_->settings().get("editor.thumbnailSize").get<float>()*dpi_;
    if(wide&&!compact){
        ImGui::BeginChild("content-tree",{metrics.directoryTreeWidth,0},true);
        const auto drawDirectory=[&](const std::string& mount,const std::string& path,auto&& self)->void{
            const auto reference=mount+":/"+path;
            const auto label=path.empty()?mount:std::filesystem::u8path(path).filename().u8string();
            bool children=false;for(const auto& entry:directories.tree())if(entry.at("mount")==mount){const auto candidate=entry.at("path").get<std::string>();if(!candidate.empty()&&std::filesystem::u8path(candidate).parent_path().generic_u8string()==path){children=true;break;}}
            auto flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth;
            if(!children)flags|=ImGuiTreeNodeFlags_Leaf;if(assetMount_==mount&&assetDirectory_==path)flags|=ImGuiTreeNodeFlags_Selected;
            if(path.empty())flags|=ImGuiTreeNodeFlags_DefaultOpen;
            const bool open=ImGui::TreeNodeEx(reference.c_str(),flags,"%s",label.c_str());observeWidget("directory."+reference);
            if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen())navigate(mount,path);
            if(open){for(const auto& entry:directories.tree())if(entry.at("mount")==mount){const auto candidate=entry.at("path").get<std::string>();if(!candidate.empty()&&std::filesystem::u8path(candidate).parent_path().generic_u8string()==path)self(mount,candidate,self);}ImGui::TreePop();}
        };
        for(const auto& entry:directories.tree())if(entry.at("path")=="")drawDirectory(entry.at("mount").get<std::string>(),{},drawDirectory);
        ImGui::EndChild();ImGui::SameLine();
    }
    visibleAssets_=nlohmann::json::array();ImGui::BeginChild("asset-list",{0,0});std::size_t requestedPreviews=0;
    if(!compact)for(const auto& directory:snapshot->directories){
        const auto reference=directory.at("reference").get<std::string>();
        if(ImGui::Selectable(("[Folder] "+reference).c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))navigate(directory.at("mount").get<std::string>(),directory.at("path").get<std::string>());
        observeWidget("folder."+reference);
    }
    const int columns=renderGrid?std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/(thumbnailSize+28*dpi_))):4;
    if(!resources.empty()&&ImGui::BeginTable("assets",columns,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY)){
        if(!renderGrid){for(const auto* title:{"资源","类型","状态","引用"})ImGui::TableSetupColumn(title);if(!compact)ImGui::TableHeadersRow();}
        ImGuiListClipper clipper;const int perRow=renderGrid?columns:1;
        const float rowHeight=renderGrid?thumbnailSize+52*dpi_:ImGui::GetTextLineHeightWithSpacing()+ImGui::GetStyle().CellPadding.y*2;
        clipper.Begin(static_cast<int>((resources.size()+perRow-1)/perRow),rowHeight);
        while(clipper.Step())for(int clippedRow=clipper.DisplayStart;clippedRow<clipper.DisplayEnd;++clippedRow){
        for(int column=0;column<perRow;++column){const auto assetIndex=static_cast<std::size_t>(clippedRow*perRow+column);if(assetIndex>=resources.size())break;
            const auto& row=resources[assetIndex];
            const auto id=row.at("id").get<std::string>(),label=row.at("path").get<std::string>(),type=row.at("type").get<std::string>();
            const auto path=std::filesystem::u8path(row.at("source").get<std::string>());const bool ready=row.at("ready").get<bool>();
            visibleAssets_.push_back(row);ImGui::PushID(id.c_str());
            if(renderGrid)ImGui::TableNextColumn();else{ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
            bool thumbnailDrawn=false;
            if(renderGrid&&(type=="model"||type=="material"||type=="image")&&requestedPreviews<2&&ImGui::IsRectVisible({thumbnailSize,thumbnailSize})&&session_->developerServices().image){
                ++requestedPreviews;const auto result=session_->edit("preview.asset",{{"asset",id}});
                if(result&&result.value.contains("handle"))if(const auto texture=previewTexture(result.value.at("handle").get<std::uint64_t>())){ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{thumbnailSize,thumbnailSize});thumbnailDrawn=true;}
            }
            if(renderGrid&&!thumbnailDrawn)ui::previewPlaceholder(type.c_str(),{thumbnailSize,thumbnailSize});
            const auto registered=types.find(type);const bool placeable=registered!=types.end()&&registered->second.placeable;
            if(ImGui::Selectable(label.c_str(),selectedAssets_.count(id)!=0,ImGuiSelectableFlags_AllowDoubleClick,{ImGui::GetContentRegionAvail().x,renderGrid?44*dpi_:0})) {
                if(ImGui::GetIO().KeyCtrl){if(selectedAssets_.count(id))selectedAssets_.erase(id);else selectedAssets_.insert(id);}else selectedAssets_={id};
                if(session_->references().active())session_->edit("reference.deliver",{{"kind","asset"},{"id",id}});
                else if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                    const auto opened=session_->edit("asset.open",{{"asset",id}});
                    if(opened){const auto operation=opened.value.at("operation").get<std::string>();
                        const auto result=session_->edit(operation,operation=="document.open"?nlohmann::json{{"path",id}}:operation=="texture.describe"?nlohmann::json{{"assets",nlohmann::json::array({id})}}:nlohmann::json{{"asset",id}});
                        if(result){if(operation=="texture.describe"){textureSnapshot_=result.value;textureFields_.clear();textureDialogRequested_=true;}
                        else if(operation=="material.describe"){materialAsset_=id;materialSnapshot_=result.value;materialDialogRequested_=true;}
                        else if(result.value.contains("handle")){assetPreviewHandle_=result.value.at("handle").get<std::uint64_t>();assetPreviewTitle_=label;assetPreviewRequested_=true;}
                        else if(operation!="document.open"){dependencySnapshot_=result.value;dependencyDialogRequested_=true;}}
                    }
                }
            }
            if(id==reveal)ImGui::SetScrollHereY(.5F);
            observeWidget("asset."+id);
            if(ImGui::BeginPopupContextItem("Asset actions")) {
                if(!selectedAssets_.count(id))selectedAssets_={id};
                if(placeable){if(ImGui::MenuItem("Place in scene"))session_->edit("asset.place",{{"asset",id},{"origin",{0,5,0}},{"direction",{0,-1,0}}});observeWidget("asset.place");}
                if(ImGui::MenuItem("Reveal in file manager"))session_->edit("path.reveal",{{"path",path.u8string()}});observeWidget("asset.reveal-file");
                const bool writable=view.isProject()&&label.rfind("engine:/",0)!=0;
                ImGui::BeginDisabled(!writable||session_->playing()||session_->building()||view.importing()||session_->reimporting());
                for(const auto& option:std::vector<std::pair<const char*,const char*>>{{"Rename","asset.rename"},{"Move","asset.move"},{"Duplicate","asset.duplicate"}}) {
                    if(ImGui::MenuItem(option.first)){contentOperation_=option.second;contentRequest_={{"asset",id}};contentValue_=option.second==std::string("asset.rename")?path.filename().u8string():label;contentDialogRequested_=true;}
                    observeWidget(option.second);
                }
                if(ImGui::MenuItem("Duplicate with dependencies")){contentOperation_="asset.duplicate";contentRequest_={{"asset",id},{"includeDependencies",true}};contentValue_=label;contentDialogRequested_=true;}observeWidget("asset.duplicate-tree");
                if(ImGui::MenuItem("Delete selected")) {
                    const auto result=session_->edit("asset.delete",{{"assets",selectedAssets_},{"referencePolicy","reject"},{"dryRun",true}});
                    if(result){contentOperation_="asset.delete";contentImpact_=result.value;}
                }observeWidget("asset.delete");
                if(ImGui::MenuItem("Reimport")) {
                    const auto& record=view.assets().records().at(id);
                    session_->edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint",std::to_string(record.fingerprint)}});
                }observeWidget("asset.reimport");
                if(ImGui::MenuItem("Export asset",nullptr,false,!session_->context().dirty())){exportRequest_={{"asset",id}};exportDialogRequested_=true;}observeWidget("asset.export");
                if(type=="image") {
                    if(ImGui::MenuItem("Texture settings")) {
                        const auto result=session_->edit("texture.describe",{{"assets",selectedAssets_}});
                        if(result){textureSnapshot_=result.value;textureFields_.clear();textureDialogRequested_=true;}
                    }observeWidget("asset.texture-settings");
                }ImGui::EndDisabled();
                if(ImGui::MenuItem("Asset diagnostics")) {
                    const auto result=session_->edit("asset.inspect",{{"asset",id}});
                    if(result){dependencySnapshot_=result.value;dependencyDialogRequested_=true;}
                }observeWidget("asset.inspect");
                for(const auto* direction:{"forward","reverse"}) {
                    if(ImGui::MenuItem(direction==std::string("forward")?"Dependencies":"Referenced by")) {
                        const auto result=session_->edit("asset.dependencies",{{"asset",id},{"direction",direction}});
                        if(result){dependencySnapshot_=result.value;dependencyDialogRequested_=true;}
                    }observeWidget(std::string("asset.dependencies.")+direction);
                }ImGui::EndPopup();
            }
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\n%s",label.c_str(),path.u8string().c_str(),ready?"Ready":"Missing source");
            if(ready&&ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("AZURE_RESOURCE",id.c_str(),id.size()+1);ImGui::TextUnformatted(label.c_str());ImGui::EndDragDropSource();}
            if(!renderGrid){ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(type.c_str());ImGui::TableSetColumnIndex(2);ui::statusText(ready?"就绪":"源文件缺失",ready?ui::StatusKind::Success:ui::StatusKind::Error);ImGui::TableSetColumnIndex(3);ImGui::Text("%zu",row.at("users").get<std::size_t>());}
            ImGui::PopID();
        }}ImGui::EndTable();
    }
    if(resources.empty()){
        if(!compact||(!assetFilter_.IsActive()&&assetTypeId_.empty()))
            ui::emptyState("没有匹配的资源",compact?"":"调整搜索、类型或目录，也可导入和创建资源。");
        if(assetFilter_.IsActive()||!assetTypeId_.empty()){
            if(ui::button(compact?"无匹配 · 清除筛选":"清除筛选")){assetFilter_.Clear();assetTypeId_.clear();}observeWidget("assets.clear-filters");
        }
    }
    ImGui::EndChild();drawTextureAssetPanel();drawMaterialAssetPanel();
    if(assetPreviewRequested_){ImGui::OpenPopup("Asset preview");assetPreviewRequested_=false;}
    if(ImGui::BeginPopupModal("Asset preview",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted(assetPreviewTitle_.c_str());
        if(const auto texture=previewTexture(assetPreviewHandle_))ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{320*dpi_,320*dpi_});
        if(ui::button("Close preview")){assetPreviewHandle_=0;ImGui::CloseCurrentPopup();}observeWidget("asset.close-preview");ImGui::EndPopup();
    }ImGui::End();
}


}
#endif
