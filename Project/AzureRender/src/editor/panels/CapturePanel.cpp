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
void ImGuiEditorLayer::drawCapturePanel(PanelContext&) {
    if(!ImGui::Begin("Capture###capture",workspace_.open("capture"))){ImGui::End();return;}
    std::array<char, 128> label{};
    const std::string& current = session_->captureLabel();
    std::memcpy(label.data(), current.data(),
        std::min(current.size(), label.size() - 1));
    if (ImGui::InputText("Label", label.data(), label.size())) {
        session_->setCaptureLabel(label.data());
    }
    if (ui::button("Capture Viewport")) {
        static_cast<void>(session_->execute(EditorCommand::Capture));
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("PNG + semantic label");
    ImGui::End();
}

}
#endif
