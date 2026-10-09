#pragma once
#include "UiMetrics.hpp"
#include "editor/properties/PropertyEditorRegistry.hpp"
#include "ThemeTokens.hpp"
#include <functional>
#include <string>
namespace azurerender::ui {
enum class ButtonVariant { Default, Toolbar, Primary, Danger };
struct ButtonOptions { ButtonVariant variant=ButtonVariant::Default;bool selected=false,disabled=false;std::string tooltip;ImVec2 size{0,0}; };
PropertyEditEvent propertyEditor(const reflection::Property& field,const nlohmann::json& state,const nlohmann::json& defaults,float scale,const std::function<void(const std::string&)>& observe);
bool button(const char* label,const ButtonOptions& options={});
bool inputText(const char* label,std::string& value,ImGuiInputTextFlags flags=0);
void propertyRow(const char* label,const UiMetrics& metrics,const std::function<void()>& draw);
void resultMessage(const std::string& message,bool failed);
}
