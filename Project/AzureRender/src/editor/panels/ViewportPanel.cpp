#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/ObjectCreationMenu.hpp"
#include "editor/GameplayDebugGeometry.hpp"
#include "runtime/InputPreferences.hpp"
#include "render/RenderMath.hpp"
#include "render/CameraProjection.hpp"
#include "reflection/Registry.hpp"
#include "ecs/Components.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height){
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawViewportPanel(PanelContext&) {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.20F, 0.0F, 0.56F, 0.72F);
#endif
    const auto metrics=ui::UiMetrics::fromScale(dpi_);
    const bool compact=EditorWorkspace::layout(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y,dpi_,session_->settings().get("editor.compact").get<bool>()).compact;
    if(compact){ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{metrics.padding,0});ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{metrics.padding,0});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{metrics.spacing,metrics.axisSpacing});}
    if(!ImGui::Begin("Viewport###viewport",workspace_.open("viewport"))){cancelViewportGizmo();navigationButton_=-1;setNavigationCapture(false);ImGui::End();if(compact)ImGui::PopStyleVar(3);return;}
    drawDocumentTabs();
    viewportFocused_ = ImGui::IsWindowFocused(
        ImGuiFocusedFlags_RootAndChildWindows);
    if (!viewportTextures_.empty()) {
        ImGui::BeginDisabled(session_->playing()||session_->building());
        for(const auto& entry:std::vector<std::pair<const char*,unsigned>>{{"Hand",3},{"Move",0},{"Rotate",1},{"Scale",2}}) {
            if(ui::button(entry.first,{ui::ButtonVariant::Toolbar,static_cast<unsigned>(context_->gizmoMode())==entry.second}))session_->edit("viewport.gizmo-mode",{{"value",entry.second}});
            observeWidget(std::string("mode.")+entry.first);ImGui::SameLine();
        }
        ImGui::EndDisabled();ImGui::NewLine();
        if(ui::button("聚焦选择",{ui::ButtonVariant::Toolbar,false,context_->selectedNodes().empty()})) {
            session_->edit("viewport.frame-selection");
        }observeWidget("focus");ui::nextToolbarItem(ui::buttonWidth("正交"));
        if(ui::button(session_->viewportView().projection==CameraProjectionKind::Orthographic?"正交":"透视",{ui::ButtonVariant::Toolbar,session_->viewportView().projection==CameraProjectionKind::Orthographic}))ImGui::OpenPopup("Viewport view options");observeWidget("viewport.view-options");ui::nextToolbarItem(ui::buttonWidth("变换选项"));
        if(ImGui::BeginPopup("Viewport view options")){
            ImGui::BeginDisabled(!session_->editCameraActive()||session_->building()||session_->gizmo().active());
            for(const auto* axis:{"+X","-X","+Y","-Y","+Z","-Z"}){if(ImGui::MenuItem(axis))session_->edit("viewport.orientation",{{"axis",axis}});observeWidget(std::string("viewport.orientation.")+axis);}
            ImGui::Separator();for(const auto* kind:{"perspective","orthographic"}){if(ImGui::MenuItem(kind))session_->edit("viewport.projection",{{"value",kind}});observeWidget(std::string("viewport.projection.")+kind);}
            ImGui::Separator();
            for(const auto& provider:session_->viewportOverlays().describe()){
                const auto category=provider.at("category").get<std::string>();
                if(ImGui::BeginMenu(provider.at("label").get<std::string>().c_str())){for(const auto* policy:{"hidden","selected","all"}){
                    if(ImGui::MenuItem(policy,nullptr,session_->viewportOverlayPolicies()[category]==policy))session_->edit("viewport.overlay-policy",{{"category",category},{"policy",policy}});
                    observeWidget("viewport.overlay."+category+"."+policy);
                }ImGui::EndMenu();}observeWidget("viewport.overlay."+category);
            }
            ImGui::EndDisabled();ImGui::EndPopup();
        }
        if(ui::button("变换选项",{ui::ButtonVariant::Toolbar}))ImGui::OpenPopup("Transform options");observeWidget("gizmo.options");
        if(ImGui::BeginPopup("Transform options")) {
            ImGui::BeginDisabled(session_->gizmo().active());
            for(const auto* space:{"world","local"}) {
                if(ImGui::Selectable(space,(context_->gizmoSpace()==EditorContext::GizmoSpace::World)==(std::string(space)=="world")))session_->edit("viewport.gizmo-options",{{"space",space}});
                observeWidget(std::string("gizmo.space.")+space);
            }
            ImGui::Separator();
            for(const auto* pivot:{"active","bounds"}) {
                if(ImGui::Selectable(pivot,(context_->gizmoPivot()==EditorContext::GizmoPivot::Active)==(std::string(pivot)=="active")))session_->edit("viewport.gizmo-options",{{"pivot",pivot}});
                observeWidget(std::string("gizmo.pivot.")+pivot);
            }
            bool snap=session_->settings().get("editor.gizmo.snap").get<bool>();
            if(ImGui::Checkbox("Snap (Ctrl)",&snap))session_->edit("settings.set",{{"name","editor.gizmo.snap"},{"value",snap}});observeWidget("gizmo.snap");
            for(const auto& entry:std::vector<std::pair<const char*,const char*>>{{"Move step","editor.gizmo.moveStep"},{"Rotate degrees","editor.gizmo.rotateStep"},{"Scale step","editor.gizmo.scaleStep"}}) {
                float step=session_->settings().get(entry.second).get<float>();
                if(ImGui::InputFloat(entry.first,&step))session_->edit("settings.set",{{"name",entry.second},{"value",step}});
            }
            ImGui::EndDisabled();ImGui::EndPopup();
        }
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
        const std::uint32_t desiredWidth = static_cast<std::uint32_t>(
            std::max(std::floor(available.x * framebufferScale.x), 64.0F));
        const std::uint32_t desiredHeight = static_cast<std::uint32_t>(
            std::max(std::floor(available.y * framebufferScale.y), 64.0F));
        if (desiredWidth == resizeCandidateWidth_
            && desiredHeight == resizeCandidateHeight_) {
            resizeStableFrames_ = std::min(resizeStableFrames_ + 1, 60U);
        } else {
            resizeCandidateWidth_ = desiredWidth;
            resizeCandidateHeight_ = desiredHeight;
            resizeStableFrames_ = 0;
        }
        if (resizeStableFrames_ >= 4
            && (desiredWidth != viewportWidth_
                || desiredHeight != viewportHeight_)
            && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            viewportResizePending_ = true;
        }
        const float aspect = static_cast<float>(viewportWidth_)
            / static_cast<float>(viewportHeight_);
        ImVec2 imageSize{available.x, available.x / aspect};
        if (imageSize.y > available.y) {
            imageSize.y = available.y;
            imageSize.x = available.y * aspect;
        }
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos({
            cursor.x + std::max((available.x - imageSize.x) * 0.5F, 0.0F),
            cursor.y + std::max((available.y - imageSize.y) * 0.5F, 0.0F),
        });
        ImGui::Image(
            reinterpret_cast<ImTextureID>(
                viewportTextures_[viewportImageIndex_]),
            imageSize);
        if(!session_->playing() && !session_->building() && ImGui::BeginDragDropTarget()){
            if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE")){
                const auto minimum=ImGui::GetItemRectMin();const auto mouse=ImGui::GetMousePos();
                const auto ray=cameraPickRay(session_->viewportView(),(mouse.x-minimum.x)/imageSize.x,(mouse.y-minimum.y)/imageSize.y,imageSize.x/imageSize.y);
                session_->edit("asset.place",{{"asset",static_cast<const char*>(payload->Data)},{"origin",ray.origin},{"direction",ray.direction}});
            }
            ImGui::EndDragDropTarget();
        }
        // Draw the viewport gizmo handles (if any selected primitive has a
        // valid screen projection). We compute endpoints here so the click
        // and drag logic can hit-test against them.
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        imageRect_={itemMin.x,itemMin.y,imageSize.x,imageSize.y};
        widgets_["viewport.image"]=imageRect_;
        for(auto item=widgets_.begin();item!=widgets_.end();)if(item.key().rfind("pick.",0)==0)item=widgets_.erase(item);else ++item;
        if(context_->isProject()) {
            for(const auto& target:context_->pickTargets()) {
                const auto projected=context_->projectDebugPoint(target.second);
                if(projected)widgets_["pick."+target.first]={itemMin.x+(*projected)[0]*imageSize.x-1,itemMin.y+(*projected)[1]*imageSize.y-1,2,2};
            }
        }
        if(session_->debugOverlay){
            auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(itemMin,{itemMin.x+imageSize.x,itemMin.y+imageSize.y},true);
            auto line=[&](const DebugLine& edge,ImU32 color){auto a=context_->projectDebugPoint(edge.from),b=context_->projectDebugPoint(edge.to);
                if(a&&b){draw->AddLine({itemMin.x+(*a)[0]*imageSize.x,itemMin.y+(*a)[1]*imageSize.y},{itemMin.x+(*b)[0]*imageSize.x,itemMin.y+(*b)[1]*imageSize.y},color,1.5F);++debugLineCount_;}};
            const auto scene=session_->viewScene();auto* runtime=session_->runtime();const auto registry=reflection::makeRuntimeRegistry();
            for(const auto& node:scene.nodes){
                ecs::TransformComponent transform{node.translation,node.rotation,node.scale};
                game::RigidBody body;game::Character character;bool hasBody=false,hasCharacter=false;
                if(runtime){const auto entity=runtime->entity(node.id);if(const auto* value=runtime->world().tryGet<game::RigidBody>(entity)){body=*value;hasBody=true;}if(const auto* value=runtime->world().tryGet<game::Character>(entity)){character=*value;hasCharacter=true;}}
                else{auto data=context_->componentData(node.id,"azure.rigid-body");if(!data.is_null()){registry.decode("azure.rigid-body",&body,{{"type","azure.rigid-body"},{"version",1},{"data",data}});hasBody=true;}
                    data=context_->componentData(node.id,"azure.character");if(!data.is_null()){registry.decode("azure.character",&character,{{"type","azure.character"},{"version",registry.type("azure.character").version},{"data",data}});hasCharacter=true;}}
                if(hasBody)for(const auto& edge:debugRigidBody(transform,body))line(edge,body.trigger?IM_COL32(255,210,60,220):IM_COL32(70,220,120,220));
                if(hasCharacter)for(const auto& edge:debugCapsule(transform,character))line(edge,IM_COL32(80,180,255,230));
            }
            if(auto* game=session_->game();game && game->hasCamera())line({game->camera().target(),game->camera().position()},IM_COL32(255,80,170,255));
            draw->PopClipRect();
        }
        if(session_->playing() && gameUi_ && imageSize.x>0 && imageSize.y>0){
            const auto mouse=ImGui::GetMousePos();const bool consumed=gameUi_->pointer(static_cast<int>((mouse.x-itemMin.x)*viewportWidth_/imageSize.x),static_cast<int>((mouse.y-itemMin.y)*viewportHeight_/imageSize.y),ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));
            if(consumed||gameUi_->wantsKeyboard())viewportAcceptsShortcuts_=false;
            if(viewportFocused_){for(auto character:ImGui::GetIO().InputQueueCharacters)gameUi_->character(character);}
        }
        const bool hovered=ImGui::IsItemHovered();
        drawViewportOverlays(itemMin,imageSize);
        const bool orientationOver=drawViewportOrientation(itemMin,imageSize);
        const bool gizmoOver=orientationOver||drawViewportGizmo(itemMin,imageSize);
        const auto& navIo=ImGui::GetIO();
        const bool navigationAllowed=session_->editCameraActive() && !session_->building() && !navIo.WantTextInput
            && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)
            && !navIo.AppFocusLost && !viewportGizmoDragActive_;
        if(!navigationAllowed)navigationButton_=-1;
        if(navigationAllowed && hovered && !orientationOver && navigationButton_<0) {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))navigationButton_=ImGuiMouseButton_Right;
            else if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))navigationButton_=ImGuiMouseButton_Middle;
            else if(navIo.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))navigationButton_=ImGuiMouseButton_Left;
            if(navigationButton_>=0){navigationMoved_=false;ImGui::SetWindowFocus();viewportFocused_=true;}
        }
        if(navigationButton_>=0&&ImGui::IsMouseDragging(navigationButton_,3*dpi_))navigationMoved_=true;
        if(navigationButton_>=0 && (!ImGui::IsMouseDown(navigationButton_) || ImGui::IsKeyPressed(ImGuiKey_Escape,false))){
            const bool contextClick=navigationButton_==ImGuiMouseButton_Right&&!navigationMoved_&&ImGui::IsMouseReleased(ImGuiMouseButton_Right);
            navigationButton_=-1;if(contextClick)ImGui::OpenPopup("Viewport context");
        }
        setNavigationCapture(navigationButton_==ImGuiMouseButton_Right);
        if(ImGui::BeginPopup("Viewport context")){
            if(ImGui::MenuItem("聚焦选择","F",false,!session_->selection().selected().empty()))session_->edit("viewport.frame-selection");
            if(ImGui::MenuItem("复制","Ctrl+D",false,!session_->selection().selected().empty()))session_->edit("node.duplicate");
            observeWidget("viewport.duplicate");
            if(ImGui::MenuItem("删除","Delete",false,!session_->selection().selected().empty()))session_->edit("node.delete");
            observeWidget("viewport.delete");
            if(ImGui::BeginMenu("创建对象")){ui::objectCreationMenu(session_->nodePresets().describe(),[&](const auto& preset){session_->edit("node.create-preset",{{"preset",preset}});});ImGui::EndMenu();}
            ImGui::EndPopup();
        }
        if (hovered || navigationButton_>=0 || viewportGizmoDragActive_) {
            const ImGuiIO& io = ImGui::GetIO();
            if(navigationAllowed && navigationButton_==ImGuiMouseButton_Right){if(io.MouseWheel!=0){const auto changed=session_->edit("viewport.fly-speed",{{"steps",io.MouseWheel}});if(changed)settingsSaveRequested_=true;}}
            else if(navigationAllowed && hovered && !orientationOver)viewportInput_.zoomSteps += io.MouseWheel;
            const float sx=session_->settings().get("editor.camera.invertX").get<bool>()?-1.F:1.F;
            const float sy=session_->settings().get("editor.camera.invertY").get<bool>()?-1.F:1.F;
            const auto sensitivity=session_->settings().get("editor.camera.lookDegreesPerPixel").get<float>()*.01745329252F/.008F;
            const auto mouseDelta=navigationButton_>=0 && !ImGui::IsMouseClicked(navigationButton_)?io.MouseDelta:ImVec2{0,0};
            if(navigationButton_==ImGuiMouseButton_Right) {
                viewportInput_.lookDeltaX+=mouseDelta.x*sensitivity*sx;
                viewportInput_.lookDeltaY+=mouseDelta.y*sensitivity*sy;
                viewportInput_.flyForward=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_W))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_S));
                viewportInput_.flyRight=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_D))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_A));
                viewportInput_.flyUp=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_E))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_Q));
                viewportInput_.flySpeed=session_->settings().get("editor.camera.flySpeed").get<float>()*(io.KeyShift?session_->settings().get("editor.camera.boost").get<float>():1.F);
                viewportInput_.deltaSeconds=io.DeltaTime;
            }else if(navigationButton_==ImGuiMouseButton_Left) {
                const auto orbitSensitivity=session_->settings().get("editor.camera.orbitDegreesPerPixel").get<float>()*.01745329252F/.008F*session_->settings().get("editor.camera.orbitSensitivity").get<float>();
                viewportInput_.orbitDeltaX+=mouseDelta.x*orbitSensitivity*sx;
                viewportInput_.orbitDeltaY+=mouseDelta.y*orbitSensitivity*sy;
            }else if(navigationButton_==ImGuiMouseButton_Middle){viewportInput_.panDeltaX+=mouseDelta.x;viewportInput_.panDeltaY+=mouseDelta.y;}

            const bool pickThisClick=!gizmoOver && !viewportGizmoDragActive_;
            if (session_->editCameraActive() && !session_->building() && pickThisClick
                && navigationButton_<0 && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mousePosition = io.MousePos;
                if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
                    viewportInput_.pickX = std::clamp(
                        (mousePosition.x - itemMin.x) / imageSize.x,
                        0.0F,
                        1.0F);
                    viewportInput_.pickY = std::clamp(
                        (mousePosition.y - itemMin.y) / imageSize.y,
                        0.0F,
                        1.0F);
                    viewportInput_.pickRequested = true;
                    viewportInput_.pickAdditive=io.KeyCtrl;
                }
            }

        }
    }
    if(!viewportTextures_.empty()){
        auto* draw=ImGui::GetWindowDrawList();const bool flying=navigationButton_==ImGuiMouseButton_Right;
        const float speed=session_->settings().get("editor.camera.flySpeed").get<float>()*(flying&&ImGui::GetIO().KeyShift?session_->settings().get("editor.camera.boost").get<float>():1.F);
        char status[160];if(flying)std::snprintf(status,sizeof(status),"Fly | %.2f m/s\nWASD  E/Q  Shift  Wheel speed  Esc release",static_cast<double>(speed));
        else std::snprintf(status,sizeof(status),"%s | RMB: fly / look\nWheel: zoom | F: frame",session_->viewportView().projection==CameraProjectionKind::Orthographic?"Orthographic":"Perspective");
        const auto theme=ui::ThemeTokens::dark();auto background=theme.recessed;background.w=.84F;
        const ImVec2 lower{imageRect_[0]+imageRect_[2],imageRect_[1]+imageRect_[3]},hud{imageRect_[0]+metrics.padding,lower.y-46*dpi_};
        draw->PushClipRect({imageRect_[0],imageRect_[1]},lower,true);draw->AddRectFilled({hud.x-4*dpi_,hud.y-2*dpi_},{std::min(lower.x,hud.x+330*dpi_),hud.y+42*dpi_},ImGui::ColorConvertFloat4ToU32(background),4*dpi_);draw->AddText(hud,ImGui::ColorConvertFloat4ToU32(theme.text),status);draw->PopClipRect();
    }
    viewportAcceptsShortcuts_ = (viewportFocused_ || navigationButton_>=0)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive();
    ImGui::End();
    if(compact)ImGui::PopStyleVar(3);
}

}
#endif
