#include "editor/ImGuiEditorLayer.hpp"
#include "render/CameraProjection.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::processFileDrops(){
    if(fileDrops_.empty()||session_->dropImporting()||session_->context().importing())return;
    auto& captured=fileDrops_.front();const auto& event=captured.event;
    if(captured.document!=session_->context().documentId()||!session_->context().isProject()||session_->playing()||session_->building()){
        session_->log("文件拖放目标已失效，请在空闲项目中重试。");fileDrops_.pop_front();return;
    }
    const auto main=ImGui::GetMainViewport();const float x=static_cast<float>(event.x)+main->Pos.x,y=static_cast<float>(event.y)+main->Pos.y;
    const auto contains=[&](const nlohmann::json& rect){return x>=rect[0].get<float>()&&y>=rect[1].get<float>()&&x<rect[0].get<float>()+rect[2].get<float>()&&y<rect[1].get<float>()+rect[3].get<float>();};
    nlohmann::json request={{"path",event.paths.front().u8string()}};bool target=false;
    if(workspace_.visible("viewport")&&widgets_.contains("viewport.image")&&contains(widgets_.at("viewport.image"))){
        const auto ray=cameraPickRay(session_->viewportView(),(x-imageRect_[0])/imageRect_[2],(y-imageRect_[1])/imageRect_[3],imageRect_[2]/imageRect_[3]);
        request["place"]=true;request["position"]=session_->placementPosition({{"origin",ray.origin},{"direction",ray.direction}});target=true;
    }else{
        for(const auto& widget:widgets_.items())if(widget.key().rfind("node.",0)==0&&contains(widget.value())){request["place"]=true;request["parent"]=widget.key().substr(5);target=true;break;}
        if(!target&&widgets_.contains("outliner.drop-region")&&contains(widgets_.at("outliner.drop-region"))){request["place"]=true;target=true;}
        if(!target&&widgets_.contains("assets.drop-region")&&contains(widgets_.at("assets.drop-region")))target=true;
    }
    if(target)session_->edit("asset.import-drop",request);else session_->log("请将文件拖入内容区、层级或场景视口。");
    captured.event.paths.erase(captured.event.paths.begin());if(captured.event.paths.empty())fileDrops_.pop_front();
}
}
#else
namespace azurerender {void ImGuiEditorLayer::processFileDrops(){fileDrops_.clear();}}
#endif
