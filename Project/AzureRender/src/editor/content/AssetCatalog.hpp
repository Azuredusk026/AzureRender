#pragma once
#include "editor/EditorContext.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
nlohmann::json assetCatalog(const EditorContext&,const std::string& type={});
}
