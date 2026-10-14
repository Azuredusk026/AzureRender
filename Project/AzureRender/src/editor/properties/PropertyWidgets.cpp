#include "PropertyWidgets.hpp"
#include "ui/Widgets.hpp"
#include <algorithm>
namespace azurerender::ui {
PropertyEditEvent propertyEditor(const reflection::Property& field,const nlohmann::json& state,const nlohmann::json& defaults,float scale,const std::function<void(const std::string&)>& observe) {
    PropertyEditEvent result;result.value=state.at("value");
    const auto label=field.label+(field.unit.empty()?std::string():" ("+field.unit+")");
    ImGui::TextUnformatted(label.c_str());
    if(state.at("mixed").get<bool>()) {ui::nextToolbarItem(ImGui::CalcTextSize("多个值").x);ui::statusText("多个值",StatusKind::Neutral);}
    ImGui::SetNextItemWidth(-1);
    const auto sample=[&]{result.began=result.began||ImGui::IsItemActivated();result.ended=result.ended||ImGui::IsItemDeactivatedAfterEdit();};
    const auto format="%."+std::to_string(field.precision)+"f";
    if(field.kind==reflection::Kind::Vector3) {
        auto values=result.value.get<std::array<float,3>>();
        const char* axes[]={"X","Y","Z"};
        const auto available=ImGui::GetContentRegionAvail().x;
        const auto metrics=UiMetrics::fromScale(scale);
        const bool stacked=available<3*metrics.vectorAxisMinWidth;
        const auto width=stacked?available:(available-2*ImGui::GetStyle().ItemSpacing.x)/3;
        ImGui::BeginGroup();
        for(int axis=0;axis<3;++axis) {
            if(axis&&!stacked)ImGui::SameLine();ImGui::PushID(axis);ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Button,ThemeTokens::axis(axis));
            if(ImGui::Button(axes[axis],{metrics.axisButtonWidth,0})){result.changed=true;result.ended=true;result.axis=axis;result.value=defaults.at(axis);}
            observe("axis.reset."+field.name+"."+std::to_string(axis));ImGui::PopStyleColor();
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Reset %s to its declared default",axes[axis]);
            ImGui::SameLine(0,metrics.axisSpacing);ImGui::SetNextItemWidth(std::max(16.F,width-metrics.axisButtonWidth-metrics.axisSpacing));
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

}
