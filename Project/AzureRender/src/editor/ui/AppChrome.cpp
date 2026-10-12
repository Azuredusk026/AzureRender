#include "Widgets.hpp"
#include "Icons.hpp"
namespace azurerender::ui {
void resultMessage(const std::string& message,bool failed) {
    if(message.empty())return;
    icon(failed?Icon::Error:Icon::Success);ImGui::SameLine();
    const auto theme=ThemeTokens::dark();ImGui::PushStyleColor(ImGuiCol_Text,failed?theme.error:theme.success);
    ImGui::TextWrapped("%s",message.c_str());ImGui::PopStyleColor();
}
void sectionHeading(const char* title,const char* description) {
    ImGui::Spacing();ImGui::TextUnformatted(title);
    if(description && *description){ImGui::PushStyleColor(ImGuiCol_Text,ThemeTokens::dark().muted);ImGui::TextWrapped("%s",description);ImGui::PopStyleColor();}
    ImGui::Spacing();
}
void card(const char* id,const std::function<void()>& draw) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ThemeTokens::dark().recessed);
    const bool visible=ImGui::BeginChild(id,{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysUseWindowPadding);
    if(visible)draw();ImGui::EndChild();ImGui::PopStyleColor();ImGui::Spacing();
}
void taskFeedback(const std::string& stage,const char* detail) {
    ImGui::PushStyleColor(ImGuiCol_Text,ThemeTokens::dark().warning);ImGui::TextWrapped("%s",stage.c_str());ImGui::PopStyleColor();
    if(detail && *detail)ImGui::TextWrapped("%s",detail);
}
}
