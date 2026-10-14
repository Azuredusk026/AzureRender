#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawDocumentTabs(){
    const auto active=context_->documentId();
    const bool changed=displayedDocument_!=active;
    displayedDocument_=active;
    const auto documents=session_->documents().describe(active,false);
    bool requestClosePopup=false;
    ImGui::BeginDisabled(session_->playing()||session_->building()||session_->gizmo().active());
    if(ImGui::BeginTabBar("Scene documents",ImGuiTabBarFlags_FittingPolicyScroll)){
        for(const auto& document:documents){
            const auto id=document.at("id").get<std::string>();
            const auto path=std::filesystem::u8path(document.at("path").get<std::string>());
            const auto title=path.filename().u8string()+"###document-"+id;
            bool open=true;ImGuiTabItemFlags flags=document.at("dirty").get<bool>()?ImGuiTabItemFlags_UnsavedDocument:ImGuiTabItemFlags_None;
            if(changed&&id==active)flags|=ImGuiTabItemFlags_SetSelected;
            if(ImGui::BeginTabItem(title.c_str(),documents.size()>1?&open:nullptr,flags)){
                ImGui::EndTabItem();
            }
            observeWidget("document.tab."+path.filename().u8string());
            if(open&&id!=active&&ImGui::IsItemClicked(ImGuiMouseButton_Left))session_->edit("document.activate",{{"id",id}});
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.u8string().c_str());
            if(!open){if(document.at("dirty").get<bool>()){documentCloseTarget_=id;requestClosePopup=true;}
                else session_->edit("document.close",{{"id",id}});}
        }
        ImGui::EndTabBar();
    }
    ImGui::EndDisabled();
    if(requestClosePopup)ImGui::OpenPopup("Close modified scene");
    if(ImGui::BeginPopupModal("Close modified scene",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Save changes to this scene before closing?");
        for(const auto& choice:std::vector<std::pair<const char*,const char*>>{{"Save","save"},{"Discard","discard"},{"Cancel","cancel"}}){
            if(ui::button(choice.first)){const auto result=session_->edit("document.close",{{"id",documentCloseTarget_},{"decision",choice.second}});if(result){documentCloseTarget_.clear();ImGui::CloseCurrentPopup();}}
            observeWidget(std::string("document.close.")+choice.second);ImGui::SameLine();
        }
        ImGui::NewLine();ImGui::EndPopup();
    }
}
}
#endif
