#include "Widgets.hpp"
#include "UiScopes.hpp"
#include <algorithm>
namespace azurerender::ui {
PropertyEditEvent propertyEditor(const reflection::Property& field,const nlohmann::json& state,const nlohmann::json& defaults,float scale,const std::function<void(const std::string&)>& observe) {
    PropertyEditEvent result;result.value=state.at("value");
    const auto label=field.label+(field.unit.empty()?std::string():" ("+field.unit+")");
    ImGui::TextUnformatted(label.c_str());
    if(state.at("mixed").get<bool>()) {ImGui::SameLine();ImGui::TextDisabled("Mixed");}
    ImGui::SetNextItemWidth(-1);
    const auto sample=[&]{result.began=result.began||ImGui::IsItemActivated();result.ended=result.ended||ImGui::IsItemDeactivatedAfterEdit();};
    const auto format="%."+std::to_string(field.precision)+"f";
    if(field.kind==reflection::Kind::Vector3) {
        auto values=result.value.get<std::array<float,3>>();
        const char* axes[]={"X","Y","Z"};
        const ImVec4 colors[]={{.70F,.18F,.18F,1},{.18F,.55F,.25F,1},{.18F,.36F,.70F,1}};
        const auto width=std::max(40.F,(ImGui::GetContentRegionAvail().x-2*ImGui::GetStyle().ItemSpacing.x)/3);
        ImGui::BeginGroup();
        for(int axis=0;axis<3;++axis) {
            if(axis)ImGui::SameLine();ImGui::PushID(axis);ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Button,colors[axis]);
            if(ImGui::Button(axes[axis],{22*scale,0})){result.changed=true;result.ended=true;result.axis=axis;result.value=defaults.at(axis);}
            observe("axis.reset."+field.name+"."+std::to_string(axis));ImGui::PopStyleColor();
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Reset %s to its declared default",axes[axis]);
            ImGui::SameLine(0,2*scale);ImGui::SetNextItemWidth(std::max(16.F,width-24*scale));
            const auto axisFormat=state.at("mixedAxes").at(axis).get<bool>()?"---":format.c_str();
            if(ImGui::DragFloat("##value",&values[axis],.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum),axisFormat)){result.changed=true;result.axis=axis;result.value=values[axis];}
            observe("axis.value."+field.name+"."+std::to_string(axis));sample();ImGui::EndGroup();ImGui::PopID();
        }ImGui::EndGroup();
    }else if(result.value.is_boolean()){bool value=result.value.get<bool>();result.changed=ImGui::Checkbox("##value",&value);result.value=value;sample();}
    else if(result.value.is_number_unsigned()){auto value=result.value.get<std::uint64_t>();const auto low=static_cast<std::uint64_t>(std::max(0.,field.minimum)),high=static_cast<std::uint64_t>(field.maximum);result.changed=ImGui::DragScalar("##value",ImGuiDataType_U64,&value,1,&low,&high,state.at("mixed").get<bool>()?"---":nullptr);result.value=value;sample();}
    else if(result.value.is_number_integer()){auto value=result.value.get<std::int64_t>();const auto low=static_cast<std::int64_t>(field.minimum),high=static_cast<std::int64_t>(field.maximum);result.changed=ImGui::DragScalar("##value",ImGuiDataType_S64,&value,1,&low,&high,state.at("mixed").get<bool>()?"---":nullptr);result.value=value;sample();}
    else if(result.value.is_number()){float value=result.value.get<float>();result.changed=ImGui::DragFloat("##value",&value,.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum),state.at("mixed").get<bool>()?"---":format.c_str());result.value=value;sample();}
    else if(result.value.is_string()){auto value=result.value.get<std::string>();result.changed=inputText("##value",value,ImGuiInputTextFlags_EnterReturnsTrue);result.value=value;sample();}
    result.cancelled=ImGui::IsItemActive()&&ImGui::IsKeyPressed(ImGuiKey_Escape,false);
    return result;
}

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
