#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <set>
namespace azurerender::gameplay {
inline void fields(const nlohmann::json& config,std::initializer_list<const char*> allowed) {
    if(!config.is_object())throw std::invalid_argument("System configuration must be an object");
    std::set<std::string> names(allowed.begin(),allowed.end());
    for(const auto& item:config.items())if(!names.count(item.key()))throw std::invalid_argument("Unknown system parameter: " + item.key());
}
inline std::string text(const nlohmann::json& value,const char* key) {
    const auto result=value.at(key).get<std::string>();if(result.empty())throw std::invalid_argument("Empty system parameter: " + std::string(key));return result;
}
}
