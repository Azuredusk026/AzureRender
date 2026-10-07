#include "Widgets.hpp"
#include "UiScopes.hpp"
namespace azurerender::ui {
bool inputText(const char* label,std::string& value,ImGuiInputTextFlags flags) {
    const auto resize=[](ImGuiInputTextCallbackData* data)->int {
        auto& text=*static_cast<std::string*>(data->UserData);text.resize(static_cast<std::size_t>(data->BufTextLen));data->Buf=text.data();return 0;
    };
    return ImGui::InputText(label,value.data(),value.capacity()+1,flags|ImGuiInputTextFlags_CallbackResize,resize,&value);
}
bool button(const char* label,const ButtonOptions& options) {
    const auto theme=ThemeTokens::dark();ScopedStyle style;
    style.color(ImGuiCol_Button,options.selected?theme.selected:options.variant==ButtonVariant::Primary?theme.accent:options.variant==ButtonVariant::Danger?theme.error:theme.control)
        .color(ImGuiCol_ButtonHovered,theme.hover).color(ImGuiCol_ButtonActive,theme.selected);
    ScopedDisabled disabled(options.disabled);const auto clicked=ImGui::Button(label,options.size);
    if(!options.tooltip.empty()&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",options.tooltip.c_str());
    return clicked;
}
void propertyRow(const char* label,const UiMetrics& metrics,const std::function<void()>& draw) {
    ScopedId id(label);ImGui::TextUnformatted(label);ImGui::SameLine(metrics.propertyLabelWidth);ImGui::SetNextItemWidth(-1);draw();
}
}
