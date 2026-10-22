#include "ui/localization/Localization.hpp"
#include "CorePanels.hpp"
#include "editor/PanelContext.hpp"
#include "ui/Widgets.hpp"
#include "editor/content/AssetCatalog.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "ui/UiScopes.hpp"
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
void AssetBrowserPanel::draw(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin(ui::tr("资源###assets"),services_.workspace.open("assets"))){ImGui::End();return;}
    {const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();services_.widgets["assets.drop-region"]={p.x,p.y,s.x,s.y};}
    const auto metrics=ui::UiMetrics::fromScale(services_.dpi);
    const auto reveal=services_.session.consumeAssetReveal();
    if(!reveal.empty()) {presentation.assetFilter_.Clear();presentation.assetTypeId_.clear();presentation.assetDirectory_.clear();presentation.assetMount_.clear();presentation.selectedAssets_={reveal};}
    const bool compact=ImGui::GetContentRegionAvail().y<metrics.compactPanelHeight;
    const bool renderGrid=presentation.assetGrid_&&!compact;
    const bool wide=compact||ImGui::GetContentRegionAvail().x>metrics.widePanelWidth;
    const auto gridWidth=ImGui::GetFrameHeight()+ImGui::CalcTextSize(ui::tr("网格")).x+metrics.spacing;
    const auto searchWidth=wide?ImGui::GetContentRegionAvail().x-metrics.filterWidth-gridWidth-3*metrics.spacing-(compact?ui::buttonWidth("+")+ui::buttonWidth(ui::tr("清除"))+2*metrics.spacing:0):-1;
    ui::searchField("##Search assets",ui::tr("搜索名称或路径"),presentation.assetFilter_,searchWidth);services_.observe("assets.search");
    if(ImGui::IsItemHovered())ImGui::SetTooltip(ui::tr("按名称或路径筛选资源"));
    ui::nextToolbarItem(metrics.filterWidth);ImGui::SetNextItemWidth(metrics.filterWidth);
    const auto& types=assetTypeRegistry().types();
    const auto current=types.find(presentation.assetTypeId_);
    if(ImGui::BeginCombo("##Type",current==types.end()?ui::tr("所有类型"):current->second.label.c_str(),ImGuiComboFlags_HeightLarge)) {
        if(ImGui::Selectable(ui::tr("所有类型"),presentation.assetTypeId_.empty()))presentation.assetTypeId_.clear();services_.observe("type.All");
        for(const auto& [id,type]:types){if(ImGui::Selectable(type.label.c_str(),presentation.assetTypeId_==id))presentation.assetTypeId_=id;services_.observe("type."+type.label);}
        ImGui::EndCombo();
    }services_.observe("assets.type");ui::nextToolbarItem(gridWidth);ImGui::Checkbox(ui::tr("网格"),&presentation.assetGrid_);services_.observe("assets.grid");
    if(compact){
        ui::nextToolbarItem(ui::buttonWidth(ui::tr("清除")));
        ImGui::BeginDisabled(!presentation.assetFilter_.IsActive()&&presentation.assetTypeId_.empty());
        if(ui::button(ui::tr("清除"))){presentation.assetFilter_.Clear();presentation.assetTypeId_.clear();}
        services_.observe("assets.clear-filters");ImGui::EndDisabled();
        if(ImGui::IsItemHovered())ImGui::SetTooltip(ui::tr("清除搜索和类型筛选"));
    }
    auto& directories=services_.session.contentDirectories();
    const auto navigate=[&](const std::string& mount,const std::string& path){
        if(presentation.assetMount_==mount&&presentation.assetDirectory_==path)return;
        presentation.assetNavigationDiagnostic_.clear();
        presentation.assetMount_=mount;presentation.assetDirectory_=path;
        presentation.assetNavigation_.resize(presentation.assetNavigationIndex_+1);presentation.assetNavigation_.push_back({mount,path});++presentation.assetNavigationIndex_;
    };
    std::shared_ptr<const ContentDirectorySnapshot> snapshot;
    try{snapshot=directories.snapshot(services_.session.context(),presentation.assetMount_,presentation.assetDirectory_,presentation.assetTypeId_,presentation.assetFilter_.InputBuf);}
    catch(const std::exception& error){presentation.assetNavigationDiagnostic_=error.what();presentation.assetMount_.clear();presentation.assetDirectory_.clear();snapshot=directories.snapshot(services_.session.context(),{}, {},presentation.assetTypeId_,presentation.assetFilter_.InputBuf);}
    const auto& resources=snapshot->assets;
    presentation.contentView_={{"matchingAssets",resources.size()},{"search",presentation.assetFilter_.InputBuf},{"type",presentation.assetTypeId_},{"grid",renderGrid},{"compact",compact},{"selectedCount",presentation.selectedAssets_.size()},{"diagnostic",presentation.assetNavigationDiagnostic_}};
    const auto directoryFilter=[&]{
        if(ui::button(ui::tr("后退"),{ui::ButtonVariant::Default,false,presentation.assetNavigationIndex_==0})){const auto& entry=presentation.assetNavigation_[--presentation.assetNavigationIndex_];presentation.assetMount_=entry.first;presentation.assetDirectory_=entry.second;}services_.observe("assets.back");ui::nextToolbarItem(ui::buttonWidth(ui::tr("前进")));
        if(ui::button(ui::tr("前进"),{ui::ButtonVariant::Default,false,presentation.assetNavigationIndex_+1>=presentation.assetNavigation_.size()})){const auto& entry=presentation.assetNavigation_[++presentation.assetNavigationIndex_];presentation.assetMount_=entry.first;presentation.assetDirectory_=entry.second;}services_.observe("assets.forward");ui::nextToolbarItem(ui::buttonWidth(ui::tr("全部")));
        if(ui::button(ui::tr("全部")))navigate({},{});services_.observe("assets.all");
        if(!presentation.assetMount_.empty()){
            ui::nextToolbarItem(ui::buttonWidth(presentation.assetMount_.c_str()));if(ui::button(presentation.assetMount_.c_str()))navigate(presentation.assetMount_,{});services_.observe("assets.mount");
            std::string accumulated;for(const auto& part:std::filesystem::u8path(presentation.assetDirectory_)){
                if(!accumulated.empty())accumulated+='/';accumulated+=part.u8string();
                ui::nextToolbarItem(ui::buttonWidth(part.u8string().c_str()));ImGui::PushID(accumulated.c_str());
                if(ui::button(part.u8string().c_str()))navigate(presentation.assetMount_,accumulated);services_.observe("assets.breadcrumb."+accumulated);ImGui::PopID();
            }
        }
    };
    ui::nextToolbarItem(ui::buttonWidth(compact?"+":ui::tr("导入 / 创建")));
    if(ui::button(compact?"+":ui::tr("导入 / 创建")))ImGui::OpenPopup(ui::tr("Content operations"));services_.observe("assets.import-create");
    if(ImGui::IsItemHovered())ImGui::SetTooltip(ui::tr("Import, create and browse content"));
    ImGui::SetNextWindowSize({std::min(640*services_.dpi,ImGui::GetIO().DisplaySize.x-32*services_.dpi),0});
    if(ImGui::BeginPopup(ui::tr("Content operations"))) {
        if(compact){
            directoryFilter();ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo(ui::tr("Directory"),presentation.assetMount_.empty()?"All directories":(presentation.assetMount_+":/"+presentation.assetDirectory_).c_str())){
                if(ImGui::Selectable(ui::tr("All directories"),presentation.assetMount_.empty()))navigate({},{});
                for(const auto& directory:directories.tree())if(ImGui::Selectable(directory.at("reference").get<std::string>().c_str()))navigate(directory.at("mount").get<std::string>(),directory.at("path").get<std::string>());
                ImGui::EndCombo();
            }services_.observe("assets.directory");
            if(ui::button(ui::tr("Reload Assets")))static_cast<void>(services_.session.execute(EditorCommand::ReloadAssets));ImGui::Separator();
        }
        std::vector<std::string> importExtensions;
        importExtensions.push_back(".azuresceneexchange");
        for(const auto& [id,format]:assetFormatRegistry().formats()){(void)id;if(format.canRead)importExtensions.insert(importExtensions.end(),format.extensions.begin(),format.extensions.end());}
        services_.pathInput(ui::tr("Asset source"),presentation.importPath_,"import",false,importExtensions);
        ui::ButtonOptions import;import.disabled=view.importing()||services_.session.playing()||services_.session.building()||presentation.importPath_.empty();
        import.tooltip=ui::tr("Select a supported model, image or audio file. Import continues when this panel is closed.");
        if(ui::button(ui::tr("Import"),import)){
            const bool exchange=std::filesystem::u8path(presentation.importPath_).extension()==".azuresceneexchange";
            const auto result=services_.session.edit(exchange?"scene.import-start":"asset.import-start",{{"path",presentation.importPath_}});
            if(result)ImGui::CloseCurrentPopup();
        }services_.observe("asset.import");
        if(std::filesystem::u8path(presentation.importPath_).extension()==".azuresceneexchange")
            ImGui::TextWrapped("%s",ui::tr("场景交换包及其模型需放入项目资产目录。刷新资产后导入，任务面板可取消。"));
        if(view.importing()){ImGui::ProgressBar(view.importProgress());if(ui::button(ui::tr("Cancel Import")))services_.session.edit("asset.import-cancel");services_.observe("asset.import-cancel");}
        ui::ButtonOptions exportOptions;exportOptions.disabled=presentation.selectedAssets_.size()!=1||services_.session.context().dirty()||view.importing()||services_.session.playing()||services_.session.building()||services_.session.reimporting();
        if(ui::button(ui::tr("Export selected asset"),exportOptions)){presentation.exportRequest_={{"asset",*presentation.selectedAssets_.begin()}};presentation.exportDialogRequested_=true;ImGui::CloseCurrentPopup();}services_.observe("assets.export");
        ImGui::BeginDisabled(!view.isProject()||services_.session.playing()||services_.session.building()||services_.session.reimporting());
        if(ui::button(ui::tr("Create directory"))){presentation.contentOperation_="content.create-directory";presentation.contentRequest_={{"parent",(presentation.assetMount_.empty()?"assets":presentation.assetMount_)+":/"+presentation.assetDirectory_}};presentation.contentValue_="New Folder";presentation.contentDialogRequested_=true;ImGui::CloseCurrentPopup();}
        services_.observe("content.create-directory");
        if(ImGui::BeginMenu(ui::tr("Create material"))){
            ui::inputText("Name",presentation.materialName_);services_.observe("material.create-name");
            for(const auto& type:MaterialTypeRegistry::builtins().describe())for(const bool parameterOverride:{false,true}){
                const auto id=type.at("id").get<std::string>();
                const auto label=type.at("label").get<std::string>()+(parameterOverride?std::string(" - ")+ui::tr("参数覆盖"):std::string());
                if(ImGui::MenuItem(label.c_str())){
                    const auto destination=(presentation.assetMount_.empty()?"assets:/":presentation.assetMount_+":/")+(presentation.assetDirectory_.empty()?"":presentation.assetDirectory_+"/")+presentation.materialName_;
                    const auto created=services_.session.edit("material.create",{{"type",id},{"mode",parameterOverride?"parameters":"replace"},{"destination",destination}});
                    if(created){presentation.materialAsset_=created.value.at("asset").get<std::string>();const auto opened=services_.session.edit("material.describe",{{"asset",presentation.materialAsset_}});if(opened){presentation.materialSnapshot_=opened.value;presentation.materialDialogRequested_=true;}}
                }services_.observe("material.create."+id+(parameterOverride?".parameters":""));
            }ImGui::EndMenu();
        }services_.observe("assets.create-material");
        if(ImGui::BeginMenu(ui::tr("Generate primitive"))){
            if(ImGui::BeginCombo(ui::tr("Shape"),presentation.primitiveKind_.c_str())){for(const auto* kind:{"cube","plane","sphere","cylinder"})if(ImGui::Selectable(kind,presentation.primitiveKind_==kind))presentation.primitiveKind_=kind;ImGui::EndCombo();}services_.observe("primitive.kind");
            ImGui::InputFloat3(ui::tr("Dimensions"),presentation.primitiveSize_.data());services_.observe("primitive.size");
            ImGui::InputInt(ui::tr("Segments"),&presentation.primitiveSegments_);services_.observe("primitive.segments");
            ui::inputText(ui::tr("File name"),presentation.primitiveName_);services_.observe("primitive.name");
            if(ui::button(ui::tr("Generate asset"))){
                const auto parent=(presentation.assetMount_.empty()?"assets":presentation.assetMount_)+":/"+presentation.assetDirectory_;
                const auto output=parent+(parent.back()=='/'?"":"/")+presentation.primitiveName_;
                const auto result=services_.session.edit("asset.generate",{{"generator","azure.primitive"},{"output",output},{"parameters",{{"kind",presentation.primitiveKind_},{"size",presentation.primitiveSize_},{"segments",presentation.primitiveSegments_}}},{"license",ui::tr("Azure primitive generator v1")},{"replace",false}});
                if(result)presentation.selectedAssets_={result.value.get<std::string>()};
            }services_.observe("primitive.generate");ImGui::TextWrapped(ui::tr("Sphere and cylinder segments use a multiple of four."));ImGui::EndMenu();
        }services_.observe("assets.generate-primitive");
        if(ui::button(ui::tr("Undo content transaction")))services_.session.edit("content.undo");services_.observe("content.undo");ImGui::EndDisabled();
        ImGui::BeginDisabled(!view.isProject()||services_.session.playing()||services_.session.building()||services_.session.reimporting());
        if(ui::button(ui::tr("Redo content transaction")))services_.session.edit("content.redo");services_.observe("content.redo");ImGui::EndDisabled();
        ImGui::BeginDisabled(services_.session.playing()||services_.session.building());
        if(ImGui::BeginCombo(ui::tr("Place Prefab"),ui::tr("Choose asset"))){
            for(const auto& row:services_.session.edit("assets.catalog",{{"type","prefab"}}).value)if(ImGui::Selectable(row.at("path").get<std::string>().c_str()))
                services_.session.edit("asset.place",{{"asset",row.at("id")},{"origin",{0,5,0}},{"direction",{0,-1,0}}});
            ImGui::EndCombo();
        }ImGui::EndDisabled();
        const auto& summary=view.importSummary();if(summary.contains("vertices"))ImGui::TextWrapped(ui::tr("Imported: %zu vertices, %zu joints, %zu materials"),summary.at("vertices").get<std::size_t>(),summary.at("joints").get<std::size_t>(),summary.at("materials").get<std::size_t>());
        ImGui::EndPopup();
    }
    if(!compact){ui::nextToolbarItem(ui::buttonWidth(ui::tr("刷新资源")));if(ui::button(ui::tr("刷新资源")))static_cast<void>(services_.session.execute(EditorCommand::ReloadAssets));services_.observe("assets.reload");ui::nextToolbarItem(ui::buttonWidth(ui::tr("后退")));directoryFilter();}
    if(!presentation.assetNavigationDiagnostic_.empty())ui::resultMessage(presentation.assetNavigationDiagnostic_,true);
    if(!compact){ui::statusText((std::to_string(resources.size())+ui::tr(" 项资源 · ")+std::to_string(presentation.selectedAssets_.size())+ui::tr(" 项已选")).c_str(),ui::StatusKind::Neutral);}
    if(renderGrid){int size=services_.session.settings().get("editor.thumbnailSize").get<int>();ImGui::SetNextItemWidth(120*services_.dpi);if(ImGui::SliderInt(ui::tr("Thumbnail size"),&size,64,192))services_.session.edit("settings.set",{{"name","editor.thumbnailSize"},{"value",size},{"source","user"}});services_.observe("assets.thumbnail-size");}
    const auto thumbnailSize=services_.session.settings().get("editor.thumbnailSize").get<float>()*services_.dpi;
    if(wide&&!compact){
        ImGui::BeginChild("content-tree",{metrics.directoryTreeWidth,0},true);
        const auto drawDirectory=[&](const std::string& mount,const std::string& path,auto&& self)->void{
            const auto reference=mount+":/"+path;
            const auto label=path.empty()?mount:std::filesystem::u8path(path).filename().u8string();
            bool children=false;for(const auto& entry:directories.tree())if(entry.at("mount")==mount){const auto candidate=entry.at("path").get<std::string>();if(!candidate.empty()&&std::filesystem::u8path(candidate).parent_path().generic_u8string()==path){children=true;break;}}
            auto flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth;
            if(!children)flags|=ImGuiTreeNodeFlags_Leaf;if(presentation.assetMount_==mount&&presentation.assetDirectory_==path)flags|=ImGuiTreeNodeFlags_Selected;
            if(path.empty())flags|=ImGuiTreeNodeFlags_DefaultOpen;
            const bool open=ImGui::TreeNodeEx(reference.c_str(),flags,"%s",label.c_str());services_.observe("directory."+reference);
            if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen())navigate(mount,path);
            if(open){for(const auto& entry:directories.tree())if(entry.at("mount")==mount){const auto candidate=entry.at("path").get<std::string>();if(!candidate.empty()&&std::filesystem::u8path(candidate).parent_path().generic_u8string()==path)self(mount,candidate,self);}ImGui::TreePop();}
        };
        for(const auto& entry:directories.tree())if(entry.at("path")=="")drawDirectory(entry.at("mount").get<std::string>(),{},drawDirectory);
        ImGui::EndChild();ImGui::SameLine();
    }
    presentation.visibleAssets_=nlohmann::json::array();ImGui::BeginChild("asset-list",{0,0});std::size_t requestedPreviews=0;
    if(!compact)for(const auto& directory:snapshot->directories){
        const auto reference=directory.at("reference").get<std::string>();
        if(ImGui::Selectable(("[Folder] "+reference).c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))navigate(directory.at("mount").get<std::string>(),directory.at("path").get<std::string>());
        services_.observe("folder."+reference);
    }
    const int columns=renderGrid?std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/(thumbnailSize+28*services_.dpi))):4;
    if(!resources.empty()&&ImGui::BeginTable("assets",columns,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY)){
        if(!renderGrid){for(const auto* title:{ui::tr("资源"),ui::tr("类型"),ui::tr("状态"),ui::tr("引用")})ImGui::TableSetupColumn(title);if(!compact)ImGui::TableHeadersRow();}
        ImGuiListClipper clipper;const int perRow=renderGrid?columns:1;
        const float rowHeight=renderGrid?thumbnailSize+52*services_.dpi:ImGui::GetTextLineHeightWithSpacing()+ImGui::GetStyle().CellPadding.y*2;
        clipper.Begin(static_cast<int>((resources.size()+perRow-1)/perRow),rowHeight);
        while(clipper.Step())for(int clippedRow=clipper.DisplayStart;clippedRow<clipper.DisplayEnd;++clippedRow){
        for(int column=0;column<perRow;++column){const auto assetIndex=static_cast<std::size_t>(clippedRow*perRow+column);if(assetIndex>=resources.size())break;
            const auto& row=resources[assetIndex];
            const auto id=row.at("id").get<std::string>(),label=row.at("path").get<std::string>(),type=row.at("type").get<std::string>();
            const auto path=std::filesystem::u8path(row.at("source").get<std::string>());const bool ready=row.at("ready").get<bool>();
            presentation.visibleAssets_.push_back(row);ImGui::PushID(id.c_str());
            if(renderGrid)ImGui::TableNextColumn();else{ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
            bool thumbnailDrawn=false;
            if(renderGrid&&(type=="model"||type=="material"||type=="image")&&requestedPreviews<2&&ImGui::IsRectVisible({thumbnailSize,thumbnailSize})&&services_.session.developerServices().image){
                ++requestedPreviews;const auto result=services_.session.edit("preview.asset",{{"asset",id}});
                if(result&&result.value.contains("handle"))if(const auto texture=services_.previewTexture(result.value.at("handle").get<std::uint64_t>())){ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{thumbnailSize,thumbnailSize});thumbnailDrawn=true;}
            }
            if(renderGrid&&!thumbnailDrawn)ui::previewPlaceholder(type.c_str(),{thumbnailSize,thumbnailSize});
            const auto registered=types.find(type);const bool placeable=registered!=types.end()&&registered->second.placeable;
            if(ImGui::Selectable(label.c_str(),presentation.selectedAssets_.count(id)!=0,ImGuiSelectableFlags_AllowDoubleClick,{ImGui::GetContentRegionAvail().x,renderGrid?44*services_.dpi:0})) {
                if(ImGui::GetIO().KeyCtrl){if(presentation.selectedAssets_.count(id))presentation.selectedAssets_.erase(id);else presentation.selectedAssets_.insert(id);}else presentation.selectedAssets_={id};
                if(services_.session.references().active())services_.session.edit("reference.deliver",{{"kind","asset"},{"id",id}});
                else if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                    const auto opened=services_.session.edit("asset.open",{{"asset",id}});
                    if(opened){const auto operation=opened.value.at("operation").get<std::string>();
                        const auto result=services_.session.edit(operation,operation=="document.open"?nlohmann::json{{"path",id}}:operation=="texture.describe"?nlohmann::json{{"assets",nlohmann::json::array({id})}}:nlohmann::json{{"asset",id}});
                        if(result){if(operation=="texture.describe"){presentation.textureSnapshot_=result.value;presentation.textureFields_.clear();presentation.textureDialogRequested_=true;}
                        else if(operation=="material.describe"){presentation.materialAsset_=id;presentation.materialSnapshot_=result.value;presentation.materialDialogRequested_=true;}
                        else if(result.value.contains("handle")){presentation.assetPreviewHandle_=result.value.at("handle").get<std::uint64_t>();presentation.assetPreviewTitle_=label;presentation.assetPreviewRequested_=true;}
                        else if(operation!="document.open"){presentation.dependencySnapshot_=result.value;presentation.dependencyDialogRequested_=true;}}
                    }
                }
            }
            if(id==reveal)ImGui::SetScrollHereY(.5F);
            services_.observe("asset."+id);
            if(ImGui::BeginPopupContextItem(ui::tr("Asset actions"))) {
                if(!presentation.selectedAssets_.count(id))presentation.selectedAssets_={id};
                if(placeable){if(ImGui::MenuItem(ui::tr("Place in scene")))services_.session.edit("asset.place",{{"asset",id},{"origin",{0,5,0}},{"direction",{0,-1,0}}});services_.observe("asset.place");}
                if(ImGui::MenuItem(ui::tr("Reveal in file manager")))services_.session.edit("path.reveal",{{"path",path.u8string()}});services_.observe("asset.reveal-file");
                const bool writable=view.isProject()&&label.rfind("engine:/",0)!=0;
                ImGui::BeginDisabled(!writable||services_.session.playing()||services_.session.building()||view.importing()||services_.session.reimporting());
                for(const auto& option:std::vector<std::pair<const char*,const char*>>{{"Rename","asset.rename"},{"Move","asset.move"},{"Duplicate","asset.duplicate"}}) {
                    if(ImGui::MenuItem(option.first)){presentation.contentOperation_=option.second;presentation.contentRequest_={{"asset",id}};presentation.contentValue_=option.second==std::string("asset.rename")?path.filename().u8string():label;presentation.contentDialogRequested_=true;}
                    services_.observe(option.second);
                }
                if(ImGui::MenuItem(ui::tr("Duplicate with dependencies"))){presentation.contentOperation_="asset.duplicate";presentation.contentRequest_={{"asset",id},{"includeDependencies",true}};presentation.contentValue_=label;presentation.contentDialogRequested_=true;}services_.observe("asset.duplicate-tree");
                if(ImGui::MenuItem(ui::tr("Delete selected"))) {
                    const auto result=services_.session.edit("asset.delete",{{"assets",presentation.selectedAssets_},{"referencePolicy","reject"},{"dryRun",true}});
                    if(result){presentation.contentOperation_="asset.delete";presentation.contentImpact_=result.value;}
                }services_.observe("asset.delete");
                if(ImGui::MenuItem(ui::tr("Reimport"))) {
                    const auto& record=view.assets().records().at(id);
                    services_.session.edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint",std::to_string(record.fingerprint)}});
                }services_.observe("asset.reimport");
                if(ImGui::MenuItem(ui::tr("Export asset"),nullptr,false,!services_.session.context().dirty())){presentation.exportRequest_={{"asset",id}};presentation.exportDialogRequested_=true;}services_.observe("asset.export");
                if(type=="image") {
                    if(ImGui::MenuItem(ui::tr("Texture settings"))) {
                        const auto result=services_.session.edit("texture.describe",{{"assets",presentation.selectedAssets_}});
                        if(result){presentation.textureSnapshot_=result.value;presentation.textureFields_.clear();presentation.textureDialogRequested_=true;}
                    }services_.observe("asset.texture-settings");
                }ImGui::EndDisabled();
                if(ImGui::MenuItem(ui::tr("Asset diagnostics"))) {
                    const auto result=services_.session.edit("asset.inspect",{{"asset",id}});
                    if(result){presentation.dependencySnapshot_=result.value;presentation.dependencyDialogRequested_=true;}
                }services_.observe("asset.inspect");
                for(const auto* direction:{"forward","reverse"}) {
                    if(ImGui::MenuItem(direction==std::string("forward")?"Dependencies":ui::tr("Referenced by"))) {
                        const auto result=services_.session.edit("asset.dependencies",{{"asset",id},{"direction",direction}});
                        if(result){presentation.dependencySnapshot_=result.value;presentation.dependencyDialogRequested_=true;}
                    }services_.observe(std::string("asset.dependencies.")+direction);
                }ImGui::EndPopup();
            }
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\n%s",label.c_str(),path.u8string().c_str(),ready?"Ready":ui::tr("Missing source"));
            if(ready&&ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("AZURE_RESOURCE",id.c_str(),id.size()+1);ImGui::TextUnformatted(label.c_str());ImGui::EndDragDropSource();}
            if(!renderGrid){ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(type.c_str());ImGui::TableSetColumnIndex(2);ui::statusText(ready?ui::tr("就绪"):ui::tr("源文件缺失"),ready?ui::StatusKind::Success:ui::StatusKind::Error);ImGui::TableSetColumnIndex(3);ImGui::Text("%zu",row.at("users").get<std::size_t>());}
            ImGui::PopID();
        }}ImGui::EndTable();
    }
    if(resources.empty()){
        if(!compact&&(presentation.assetFilter_.IsActive()||!presentation.assetTypeId_.empty())){
            if(ui::button(compact?ui::tr("无匹配 · 清除筛选"):ui::tr("清除筛选"))){presentation.assetFilter_.Clear();presentation.assetTypeId_.clear();}services_.observe("assets.clear-filters");
        }
        if(!compact||(!presentation.assetFilter_.IsActive()&&presentation.assetTypeId_.empty()))
            ui::emptyState(ui::tr("没有匹配的资源"),compact?"":ui::tr("调整搜索、类型或目录，也可导入和创建资源。"));
    }
    ImGui::EndChild();services_.textureDialog();services_.materialDialog();
    if(presentation.assetPreviewRequested_){ImGui::OpenPopup(ui::tr("Asset preview"));presentation.assetPreviewRequested_=false;}
    if(ImGui::BeginPopupModal(ui::tr("Asset preview"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted(presentation.assetPreviewTitle_.c_str());
        if(const auto texture=services_.previewTexture(presentation.assetPreviewHandle_))ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{320*services_.dpi,320*services_.dpi});
        if(ui::button(ui::tr("Close preview"))){presentation.assetPreviewHandle_=0;ImGui::CloseCurrentPopup();}services_.observe("asset.close-preview");ImGui::EndPopup();
    }ImGui::End();
}


}
#endif
