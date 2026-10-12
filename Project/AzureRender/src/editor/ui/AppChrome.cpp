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
bool searchField(const char* id,const char* hint,ImGuiTextFilter& filter,float width){
    ImGui::SetNextItemWidth(width);
    const auto changed=ImGui::InputTextWithHint(id,hint,filter.InputBuf,sizeof(filter.InputBuf));
    if(changed)filter.Build();return changed;
}
float buttonWidth(const char* label){return ImGui::CalcTextSize(label,nullptr,true).x+2*ImGui::GetStyle().FramePadding.x;}
void nextToolbarItem(float width){
    const auto right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
    if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=right)ImGui::SameLine();
}
void statusText(const char* text,StatusKind kind){
    const auto theme=ThemeTokens::dark();
    const auto color=kind==StatusKind::Error?theme.error:kind==StatusKind::Warning?theme.warning:
        kind==StatusKind::Success?theme.success:kind==StatusKind::Info?theme.accent:theme.muted;
    ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TextWrapped("%s",text);ImGui::PopStyleColor();
}
void emptyState(const char* title,const char* detail){
    statusText(title,StatusKind::Neutral);
    if(detail&&*detail)statusText(detail,StatusKind::Neutral);
}
void previewPlaceholder(const char* label,ImVec2 size){
    const auto position=ImGui::GetCursorScreenPos();const auto theme=ThemeTokens::dark();
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(position,{position.x+size.x,position.y+size.y},ImGui::ColorConvertFloat4ToU32(theme.control),ImGui::GetStyle().FrameRounding);
    const auto text=ImGui::CalcTextSize(label);
    draw->PushClipRect(position,{position.x+size.x,position.y+size.y},true);
    draw->AddText({position.x+(size.x-text.x)*.5F,position.y+(size.y-text.y)*.5F},ImGui::ColorConvertFloat4ToU32(theme.muted),label);
    draw->PopClipRect();ImGui::Dummy(size);
}
}
