#include "ui/localization/Localization.hpp"
#include "editor/panels/CorePanels.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "editor/graph/MaterialGraphModel.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
namespace {
void materialGraphCanvas(EditorSession& session,const std::string& asset,const nlohmann::json& document,float scale){
    auto material=MaterialData::parse(document);
    auto graph=material.authoringGraph.is_null()?MaterialGraphModel::fromMaterial(material).encode():material.authoringGraph;
    ImGui::TextWrapped(ui::tr("拖动节点调整布局，通过连接设置输出参数和纹理。"));
    ImGui::BeginChild(ui::tr("Material graph canvas"),{0,260*scale},true,ImGuiWindowFlags_HorizontalScrollbar);
    const auto origin=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    const auto position=[&](const nlohmann::json& node){return ImVec2(origin.x+node.at("position")[0].get<float>()*scale,origin.y+node.at("position")[1].get<float>()*scale);};
    const auto output=std::find_if(graph["nodes"].begin(),graph["nodes"].end(),[](const auto& node){return node.at("id")=="output";});
    for(const auto& link:graph["links"]){
        const auto source=std::find_if(graph["nodes"].begin(),graph["nodes"].end(),[&](const auto& node){return node.at("id")==link.at("source");});
        auto a=position(*source),b=position(*output);a.x+=150*scale;a.y+=25*scale;b.y+=25*scale;
        draw->AddBezierCubic(a,{a.x+60*scale,a.y},{b.x-60*scale,b.y},b,IM_COL32(90,180,230,255),2*scale);
    }
    bool changed=false;
    for(auto& node:graph["nodes"]){
        const auto id=node.at("id").get<std::string>();ImGui::PushID(id.c_str());auto p=position(node);
        ImGui::SetCursorScreenPos(p);ImGui::InvisibleButton("node",{150*scale,50*scale});
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0)){const auto delta=ImGui::GetMouseDragDelta();p.x+=delta.x;p.y+=delta.y;}
        draw->AddRectFilled(p,{p.x+150*scale,p.y+50*scale},id=="output"?IM_COL32(85,85,95,255):IM_COL32(30,65,95,255),5*scale);
        draw->AddText({p.x+8*scale,p.y+7*scale},IM_COL32_WHITE,id.c_str());
        if(ImGui::IsItemDeactivated()){const auto delta=ImGui::GetMouseDragDelta();if(std::abs(delta.x)+std::abs(delta.y)>1){node["position"]={node["position"][0].get<float>()+delta.x/scale,node["position"][1].get<float>()+delta.y/scale};changed=true;}}
        ImGui::PopID();
    }
    ImGui::SetCursorPos({1000*scale,1400*scale});ImGui::Dummy({1,1});ImGui::EndChild();
    for(const auto& field:material.parameters.items()){
        auto& links=graph["links"];const auto found=std::find_if(links.begin(),links.end(),[&](const auto& link){return link.at("slot")==field.key();});
        const auto current=found==links.end()?std::string("Default"):found->at("source").get<std::string>();
        if(ImGui::BeginCombo((std::string(ui::tr(field.key().c_str()))+" "+ui::tr("input")+"###"+field.key()+"-input").c_str(),ui::tr(current.c_str()))){
            if(ImGui::Selectable(ui::tr("Default"),found==links.end())){if(found!=links.end())links.erase(found);changed=true;}
            for(const auto& node:graph["nodes"]){
                if(node.at("kind")!="value"||node.at("value").type()!=field.value().type())continue;
                const auto id=node.at("id").get<std::string>();
                if(ImGui::Selectable(id.c_str(),current==id)){links.erase(std::remove_if(links.begin(),links.end(),[&](const auto& link){return link.at("slot")==field.key();}),links.end());links.push_back({{"source",id},{"target","output"},{"slot",field.key()}});changed=true;}
            }ImGui::EndCombo();
        }
    }
    if(changed)session.edit("material.graph",{{"asset",asset},{"graph",graph}});
    if(material.authoringGraph.is_null()&&ui::button(ui::tr("保存节点图")))session.edit("material.graph",{{"asset",asset},{"graph",graph}});
}
}
void ImGuiEditorLayer::drawMaterialAssetPanel(){
    if(assetPanel_->presentation.materialDialogRequested_){ImGui::OpenPopup(ui::tr("Material asset"));assetPanel_->presentation.materialDialogRequested_=false;}
    const auto work=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(620*dpi_,work.x-24*dpi_),std::min(760*dpi_,work.y-24*dpi_)},ImGuiCond_Always);
    if(!ImGui::BeginPopupModal(ui::tr("Material asset"),nullptr,ImGuiWindowFlags_NoResize))return;
    const auto label=[&](const std::string& id){const auto& records=session_->context().assets().records();const auto found=records.find(id);return found==records.end()?id:found->second.virtualPath;};
    const auto refreshed=session_->edit("material.describe",{{"asset",assetPanel_->presentation.materialAsset_}});
    if(refreshed)assetPanel_->presentation.materialSnapshot_=refreshed.value;
    ImGui::BeginChild(ui::tr("Material parameters"),{0,-42*dpi_},false);
    if(assetPanel_->presentation.materialSnapshot_.contains("document")){
        const bool graphOpen=ImGui::CollapsingHeader(ui::tr("材质节点图"));observeWidget("material.graph.toggle");
        if(graphOpen)materialGraphCanvas(*session_,assetPanel_->presentation.materialAsset_,assetPanel_->presentation.materialSnapshot_.at("document"),dpi_);
        ImGui::TextUnformatted(assetPanel_->presentation.materialSnapshot_.at("document").at("typeId").get<std::string>().c_str());
        if(session_->developerServices().image){const auto preview=session_->edit("preview.asset",{{"asset",assetPanel_->presentation.materialAsset_}});if(preview)if(const auto texture=previewTexture(preview.value.at("handle").get<std::uint64_t>()))ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)),{160*dpi_,160*dpi_});}
        const auto& materialDocument=assetPanel_->presentation.materialSnapshot_.at("document");
        const bool parameterOverride=materialDocument.value("mode",std::string())=="parameters";
        if(parameterOverride)ImGui::TextWrapped(ui::tr("勾选需要覆盖的参数。其余参数和特殊效果继承源材质。"));
        for(const auto& field:assetPanel_->presentation.materialSnapshot_.at("type").items()){
            const auto& specification=field.value();const auto& parameters=materialDocument.at("parameters");
            if(parameterOverride){
                bool enabled=parameters.contains(field.key());
                if(ImGui::Checkbox((std::string(ui::tr("覆盖"))+"###override-"+field.key()).c_str(),&enabled))
                    session_->edit("material.field",{{"asset",assetPanel_->presentation.materialAsset_},{"field",field.key()},{"value",enabled?specification.at("default"):nlohmann::json(nullptr)}});
                observeWidget("material.override."+field.key());ImGui::SameLine();
                if(!parameters.contains(field.key())){ImGui::Text("%s (%s)",ui::tr(field.key().c_str()),ui::tr("继承"));continue;}
            }
            const auto value=parameters.at(field.key());const auto kind=specification.at("type").get<std::string>();
            nlohmann::json next=value;bool changed=false;
            if(kind=="color"){auto color=value.get<std::array<float,4>>();changed=ImGui::ColorEdit4(ui::tr(field.key().c_str()),color.data());next=color;}
            else if(kind=="boolean"){auto boolean=value.get<bool>();changed=ImGui::Checkbox(ui::tr(field.key().c_str()),&boolean);next=boolean;}
            else {float number=value.get<float>();changed=ImGui::SliderFloat(ui::tr(field.key().c_str()),&number,specification.at("min").get<float>(),specification.at("max").get<float>());next=number;}
            observeWidget("material.field."+field.key());
            if(changed)session_->edit("material.field",{{"asset",assetPanel_->presentation.materialAsset_},{"field",field.key()},{"value",next}});
        }
        for(const auto& slot:assetPanel_->presentation.materialSnapshot_.at("textures").items()){
            const auto& document=assetPanel_->presentation.materialSnapshot_.at("document");const auto current=document.at("textures").contains(slot.key())?document.at("textures").at(slot.key()).at("asset").get<std::string>():std::string();
            const auto emptyLabel=ui::tr(parameterOverride?"继承":"None");
            if(ImGui::BeginCombo((std::string(ui::tr(slot.key().c_str()))+" "+ui::tr("texture")+"###"+slot.key()+"-texture").c_str(),current.empty()?emptyLabel:label(current).c_str())){
                if(ImGui::Selectable(emptyLabel,current.empty()))session_->edit("material.field",{{"asset",assetPanel_->presentation.materialAsset_},{"field","textures."+slot.key()},{"value",nullptr}});
                for(const auto& [id,record]:session_->context().assets().records())if(assetTypeRegistry().matches(record.path,{"image"})){
                    if(ImGui::Selectable(record.virtualPath.c_str(),current==id))session_->edit("material.field",{{"asset",assetPanel_->presentation.materialAsset_},{"field","textures."+slot.key()},{"value",{{"asset",id},{"usage",slot.value()}}}});
                    observeWidget("material.texture."+slot.key()+"."+id);
                }ImGui::EndCombo();
            }observeWidget("material.texture."+slot.key());
        }
        ImGui::TextWrapped(ui::tr("参数修改保存到材质资源；场景材质槽分配支持撤销。"));
        for(const auto& error:session_->feedback().report())if(error.at("source").get<std::string>().rfind("material.",0)==0)ImGui::TextWrapped("%s",error.at("message").get<std::string>().c_str());
    }
    ImGui::EndChild();
    if(assetPanel_->presentation.materialSnapshot_.contains("document")){
        if(ui::button(ui::tr("Undo material"),{ui::ButtonVariant::Default,false,!assetPanel_->presentation.materialSnapshot_.at("canUndo").get<bool>()}))session_->edit("material.undo",{{"asset",assetPanel_->presentation.materialAsset_}});observeWidget("material.undo");ImGui::SameLine();
        if(ui::button(ui::tr("Redo material"),{ui::ButtonVariant::Default,false,!assetPanel_->presentation.materialSnapshot_.at("canRedo").get<bool>()}))session_->edit("material.redo",{{"asset",assetPanel_->presentation.materialAsset_}});observeWidget("material.redo");ImGui::SameLine();
    }
    if(ui::button(ui::tr("Close material"))){assetPanel_->presentation.materialAsset_.clear();assetPanel_->presentation.materialSnapshot_=nullptr;ImGui::CloseCurrentPopup();}observeWidget("material.close");ImGui::EndPopup();
}
void ImGuiEditorLayer::drawMaterialSlots(const SceneNode& node){
    if(node.resourceId.empty()||!session_->context().isProject())return;
    const bool open=ImGui::CollapsingHeader(ui::tr("材质槽"));observeWidget("material.slots-header");if(!open)return;
    const auto result=session_->edit("material.slots",{{"node",node.id}});if(!result)return;
    const auto label=[&](const std::string& id){const auto& records=session_->context().assets().records();const auto found=records.find(id);return found==records.end()?id:found->second.virtualPath;};
    for(const auto& slot:result.value.at("slots")){
        const auto id=slot.at("id").get<std::string>();ImGui::PushID(id.c_str());const auto assigned=slot.at("override").is_null()?std::string():slot.at("override").get<std::string>();
        if(ImGui::BeginCombo(slot.at("name").get<std::string>().c_str(),assigned.empty()?ui::tr("Source material"):label(assigned).c_str())){
            for(const auto& [uuid,record]:session_->context().assets().records())if(assetTypeRegistry().matches(record.path,{"material"})){
                if(ImGui::Selectable(record.virtualPath.c_str(),uuid==assigned))session_->edit("material.assign",{{"nodes",session_->selection().selected()},{"slotId",id},{"material",uuid}});
                observeWidget("material.assign."+id+"."+uuid);
            }ImGui::EndCombo();
        }observeWidget("material.slot."+id);
        if(ImGui::BeginDragDropTarget()){if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE"))session_->edit("material.assign",{{"nodes",session_->selection().selected()},{"slotId",id},{"material",std::string(static_cast<const char*>(payload->Data))}});ImGui::EndDragDropTarget();}
        if(ui::button(ui::tr("Reset slot"),{ui::ButtonVariant::Default,false,assigned.empty()}))session_->edit("material.reset-slot",{{"nodes",session_->selection().selected()},{"slotId",id}});observeWidget("material.reset."+id);
        if(!slot.at("stable").get<bool>())ImGui::TextWrapped(ui::tr("模型材质槽名称重复或缺失。可在源模型中设置 azureSlotId，保持重新导入时的对应关系。"));
        ImGui::PopID();
    }
    for(const auto& diagnostic:result.value.at("diagnostics")){
        const auto id=diagnostic.at("slot").get<std::string>();ImGui::TextWrapped(ui::tr("缺少源材质槽：%s"),id.c_str());
        if(ui::button(("Clear missing slot##"+id).c_str()))session_->edit("material.reset-slot",{{"nodes",session_->selection().selected()},{"slotId",id}});
    }
}
}
#endif
