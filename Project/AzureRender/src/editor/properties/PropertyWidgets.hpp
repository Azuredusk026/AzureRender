#pragma once
#include "PropertyEditorRegistry.hpp"
#include <functional>
namespace azurerender::ui {
PropertyEditEvent propertyEditor(const reflection::Property& field,const nlohmann::json& state,const nlohmann::json& defaults,float scale,const std::function<void(const std::string&)>& observe);
}
