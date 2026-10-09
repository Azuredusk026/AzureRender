#include "ImGuiEditorLayer.hpp"
#include "EditorTheme.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
namespace azurerender {
void ImGuiEditorLayer::drawPathInput(const char* label,std::string& value,const std::string& purpose,bool directory,const std::vector<std::string>& extensions) {
    ImGui::PushID(purpose.c_str());ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
    ui::inputText("##path",value);observeWidget(purpose+".path");
    if(ui::button("Browse...")){
        const auto result=session_->edit("path.choose",{{"purpose",purpose},{"title",label},{"directory",directory},{"extensions",extensions}});
        if(result&&!result.value.value("cancelled",true))value=result.value.at("path").get<std::string>();
    }observeWidget(purpose+".browse");ImGui::SameLine();ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##Recent","Recent directories")){
        for(const auto& path:session_->pathHistory().directories(purpose))if(ImGui::Selectable(path.c_str())){
            if(directory)value=path;
            else{
                session_->edit("path.remember",{{"purpose",purpose},{"path",path},{"directory",true}});
                const auto result=session_->edit("path.choose",{{"purpose",purpose},{"title",label},{"directory",false},{"extensions",extensions}});
                if(result&&!result.value.value("cancelled",true))value=result.value.at("path").get<std::string>();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}
void ImGuiEditorLayer::queueInputEvent(nlohmann::json event) {
    if(uiCursor_>=1024) { uiActions_.erase(uiActions_.begin(),uiActions_.begin()+static_cast<std::ptrdiff_t>(uiCursor_));uiCursor_=0; }
    if(uiActions_.size()-uiCursor_>=1024||event.dump().size()>65536)throw std::invalid_argument("UI event queue exceeds budget");
    const auto action=event.at("action").get<std::string>();
    if(action=="mouse"||action=="click") {
        const auto target=event.at("target").get<std::string>();
        if(!widgets_.contains(target))throw std::invalid_argument("Unknown UI target: "+target);
        for(const auto v:event.value("offset",std::array<float,2>{0,0}))if(!std::isfinite(v)||std::abs(v)>10000)throw std::invalid_argument("Invalid mouse offset");
        event.value("down",true);
        const auto button=event.value("button",std::string("left"));
        if(button!="left"&&button!="right"&&button!="middle")throw std::invalid_argument("Unknown mouse button");
    }else if(action=="focus")event.at("focused").get<bool>();
    else if(action=="text")event.at("text").get<std::string>();
    else if(action=="key") {
        const std::set<std::string> keys={"A","D","W","S","E","R","F","Q","P","Z","Y","Delete","Enter","Escape","Shift","Alt"};
        if(!keys.count(event.at("key").get<std::string>()))throw std::invalid_argument("Unsupported UI key");
        event.value("down",true);event.value("ctrl",false);
    }else if(action=="wheel") {
        for(const char* axis:{"x","y"})if(!std::isfinite(event.value(axis,0.F)))throw std::invalid_argument("Invalid wheel input");
    }else throw std::invalid_argument("Unknown UI event");
    event["frame"]=uiFrame_+1;uiActions_.push_back(std::move(event));
    std::stable_sort(uiActions_.begin()+static_cast<std::ptrdiff_t>(uiCursor_),uiActions_.end(),[](const auto& a,const auto& b){return a.at("frame")<b.at("frame");});
}
void ImGuiEditorLayer::observeWidget(const std::string& id) {
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    widgets_[id]={a.x,a.y,b.x-a.x,b.y-a.y};
}
void ImGuiEditorLayer::injectUiEvents() {
    if(uiActions_.empty())return;
    auto& io=ImGui::GetIO();io.AddFocusEvent(injectedFocus_);io.AddMousePosEvent(uiMousePosition_[0],uiMousePosition_[1]);
    if(injectedMouseDown_){io.AddMouseButtonEvent(injectedClickButton_,false);injectedMouseDown_=false;}
    while(uiCursor_<uiActions_.size()&&uiActions_[uiCursor_].at("frame").get<std::uint64_t>()<=uiFrame_) {
        const auto action=uiActions_[uiCursor_++];
        try {
            const auto kind=action.at("action").get<std::string>();
            if(kind=="click" || kind=="mouse") {
                const auto target=action.at("target").get<std::string>();
                const auto rect=widgets_.at(target).get<std::array<float,4>>();
                const auto offset=action.value("offset",std::array<float,2>{0,0});
                uiMousePosition_={rect[0]+rect[2]/2+offset[0],rect[1]+rect[3]/2+offset[1]};
                io.AddMousePosEvent(uiMousePosition_[0],uiMousePosition_[1]);
                const auto button=action.value("button",std::string("left"));
                const int index=button=="right"?1:button=="middle"?2:0;
                io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiMod_Ctrl),action.value("ctrl",false));
                io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiMod_Shift),action.value("shift",false));
                io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiMod_Alt),action.value("alt",false));
                io.AddMouseButtonEvent(index,action.value("down",true));injectedMouseDown_=kind=="click";injectedClickButton_=index;
            }else if(kind=="focus"){injectedFocus_=action.at("focused").get<bool>();io.AddFocusEvent(injectedFocus_);}
            else if(kind=="text")io.AddInputCharactersUTF8(action.at("text").get<std::string>().c_str());
            else if(kind=="wheel")io.AddMouseWheelEvent(action.value("x",0.F),action.value("y",0.F));
            else if(kind=="key") {
                static const std::map<std::string,ImGuiKey> keys={{"A",ImGuiKey_A},{"D",ImGuiKey_D},{"W",ImGuiKey_W},
                    {"S",ImGuiKey_S},{"E",ImGuiKey_E},{"R",ImGuiKey_R},{"F",ImGuiKey_F},{"Q",ImGuiKey_Q},{"P",ImGuiKey_P},{"Alt",ImGuiKey_LeftAlt},{"Z",ImGuiKey_Z},{"Y",ImGuiKey_Y},{"Delete",ImGuiKey_Delete},
                    {"Enter",ImGuiKey_Enter},{"Escape",ImGuiKey_Escape},{"Shift",ImGuiKey_LeftShift}};
                io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiMod_Ctrl),action.value("ctrl",false));
                io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiMod_Alt),action.value("alt",false));
                io.AddKeyEvent(keys.at(action.at("key").get<std::string>()),action.value("down",true));
            }else if(kind=="dpi") {
                dpiOverride_=action.at("scale").get<float>();dpi_=dpiOverride_;
                EditorTheme::apply(dpi_);dockingLayoutInitialized_=false;workspaceRebuildRequested_=true;
            }else throw std::invalid_argument("Unknown UI event");
        }catch(const std::exception& error){uiErrors_.push_back({{"frame",uiFrame_},{"action",action},{"error",error.what()}});}
    }
}
void ImGuiEditorLayer::drawWorkspace() {
    const auto* vp=ImGui::GetMainViewport();const auto layout=EditorWorkspace::layout(vp->Size.x,vp->Size.y,dpi_,session_->settings().get("editor.compact").get<bool>());
    const auto hostPos=ImVec2(vp->Pos.x+layout.left,vp->Pos.y+layout.menu+layout.toolbar);
    const auto hostSize=ImVec2(vp->Size.x-layout.left,vp->Size.y-layout.menu-layout.toolbar-layout.status);
    ImGui::SetNextWindowPos(hostPos);ImGui::SetNextWindowSize(hostSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    constexpr auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove
        |ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Workspace###workspace-host",nullptr,flags);
#ifdef IMGUI_HAS_DOCK
    const auto dockspace=ImGui::GetID("AzureWorkspace");
    const auto preset=session_->consumeWorkspacePreset();
    if(!preset.empty()){workspace_.preset(preset);dockingLayoutInitialized_=false;workspaceRebuildRequested_=true;}
    const bool reset=session_->consumeLayoutResetRequest();
    if(reset){workspace_.reset();dockingLayoutInitialized_=false;}
    if(!dockingLayoutInitialized_) {
        const auto* node=ImGui::DockBuilderGetNode(dockspace);
        if(reset || workspaceRebuildRequested_ || node==nullptr || node->IsEmpty()) {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace,ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodePos(dockspace,hostPos);ImGui::DockBuilderSetNodeSize(dockspace,hostSize);
            ImGuiID center=dockspace;
            const auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,layout.right/hostSize.x,nullptr,&center);
            ImGuiID details=right;
            const auto objects=ImGui::DockBuilderSplitNode(details,ImGuiDir_Up,.42F,nullptr,&details);
            const auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,layout.bottom/hostSize.y,nullptr,&center);
            for(const auto& panel:workspace_.panels()) {
                const auto target=(panel.id=="viewport"||panel.id=="projects")?center:panel.id=="outliner"?objects:(panel.id=="inspector"||panel.id=="environment"||panel.id=="settings")?details:bottom;
                ImGui::DockBuilderDockWindow(panel.title.c_str(),target);
            }
            ImGui::DockBuilderFinish(dockspace);
        }
        dockingLayoutInitialized_=true;
        workspaceRebuildRequested_=false;
    }
    ImGui::DockSpace(dockspace,{0,0},ImGuiDockNodeFlags_PassthruCentralNode);
#endif
    ImGui::End();ImGui::PopStyleVar(2);
    if(layout.left>0) {
        ImGui::SetNextWindowPos({vp->Pos.x,hostPos.y});ImGui::SetNextWindowSize({layout.left,hostSize.y});
        ImGui::Begin("Create & Project###project",nullptr,flags);
        ImGui::PushFont(nullptr,16);ImGui::TextUnformatted("PROJECT");ImGui::PopFont();
        if(context_->isProject()){ImGui::TextWrapped("%s",context_->project().name.c_str());ImGui::TextDisabled("%s",context_->scene().sceneId.c_str());}
        ImGui::Separator();ImGui::TextUnformatted("Create objects");
        ImGui::BeginDisabled(session_->playing()||session_->building());
        if(ImGui::Button("Empty Node",{-1,0}))session_->edit("node.create");observeWidget("create.empty");
        if(ImGui::Button("Duplicate Selected",{-1,0}))session_->edit("node.duplicate");
        ImGui::EndDisabled();
        ImGui::Separator();ImGui::TextUnformatted("Tools");
        for(const auto& id:{"assets","animation","gameplay-debug","build","capture","console","settings","environment","projects"}) {
            const auto& panel=*std::find_if(workspace_.panels().begin(),workspace_.panels().end(),[&](const auto& p){return p.id==id;});
            const auto title=panel.title.substr(0,panel.title.find("###"));
            if(ImGui::Button(title.c_str(),{-1,0})){workspace_.setVisible(id,true);ImGui::SetWindowFocus(panel.title.c_str());}
            observeWidget(std::string("tool.")+id);
        }
        ImGui::Separator();ImGui::TextWrapped("RMB + WASD: fly\nShift: accelerate\nAlt + LMB: orbit\nMMB: pan\nWheel: zoom\nW/E/R: move/rotate/scale\nF / outliner double click: frame\nCtrl+S: save\nCtrl+P: play / stop");
        ImGui::End();
    }
}
nlohmann::json ImGuiEditorLayer::workspaceSnapshot(bool includeHistory) const {
    nlohmann::json data={{"version",EditorWorkspace::version},{"preset",workspace_.snapshot().at("preset")},{"dpi",dpi_},{"panels",nlohmann::json::object()},
        {"image",{{"x",imageRect_[0]},{"y",imageRect_[1]},{"width",imageRect_[2]},{"height",imageRect_[3]}}},
        {"widgets",widgets_},{"uiErrors",uiErrors_},{"historyCount",uiHistory_.size()},{"diagnostic",workspace_.diagnostic},
        {"selectedName",context_->selectedNode()?context_->selectedNode()->name:""},
        {"nodeCount",context_->scene().nodes.size()},{"gizmoTranslation",context_->gizmoTranslation()},
        {"gizmoRotation",context_->gizmoRotation()},{"gizmoScale",context_->gizmoScale()},{"visibleAssets",visibleAssets_}};
    data["camera"]={{"position",cameraPosition_},{"target",cameraTarget_}};
    if(includeHistory)data["history"]=uiHistory_;
    data["gizmoSpace"]=context_->gizmoSpace()==EditorContext::GizmoSpace::World?"world":"local";
    data["gizmoPivot"]=context_->gizmoPivot()==EditorContext::GizmoPivot::Active?"active":"bounds";
    data["referencePicker"]=session_->references().active();
    data["activeSelection"]=session_->selection().active();
    data["feedback"]=session_->feedback().report();data["tasks"]=session_->tasks().report();
    data["gizmoMode"]=static_cast<unsigned>(context_->gizmoMode());
    data["capture"]={{"navigationButton",navigationButton_},{"gizmo",viewportGizmoDragActive_}};
    data["documentGuard"]=static_cast<unsigned>(session_->documentGuard().state());
    data["dirty"]=context_->dirty();data["undoCount"]=context_->undoCount();data["selection"]=session_->selection().selected();
    data["settings"]=session_->settings().describe();
    data["project"]=context_->isProject()?nlohmann::json{{"id",context_->project().id},{"name",context_->project().name},{"path",context_->project().file.u8string()}}:nlohmann::json::object();
    for(const auto& panel:workspace_.panels()) {
        const auto* window=ImGui::FindWindowByName(panel.title.c_str());
        data["panels"][panel.id]={{"open",panel.visible},{"docked",window&&window->DockId!=0},
            {"rect",window?nlohmann::json{window->Pos.x,window->Pos.y,window->Size.x,window->Size.y}:nlohmann::json::array()}};
    }
    return data;
}
}
#else
namespace azurerender {
void ImGuiEditorLayer::observeWidget(const std::string&){}
void ImGuiEditorLayer::injectUiEvents(){}
void ImGuiEditorLayer::queueInputEvent(nlohmann::json) { throw std::logic_error("UI input requires ImGui"); }
void ImGuiEditorLayer::drawWorkspace(){}
nlohmann::json ImGuiEditorLayer::workspaceSnapshot(bool) const{return {};}
}
#endif
