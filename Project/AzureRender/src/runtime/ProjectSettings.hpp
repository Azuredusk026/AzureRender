#pragma once
#include "runtime/Project.hpp"
#include "foundation/SettingRegistry.hpp"
#include <fstream>
namespace azurerender {
inline bool projectSetting(const std::string& name){return name.rfind("render.",0)==0||name.rfind("runtime.",0)==0||name.rfind("input.",0)==0;}
inline std::filesystem::path projectSettingsPath(const Project& project){return project.file.parent_path()/"config/engine-settings.json";}
inline SettingResult loadProjectSettings(SettingRegistry& registry,const Project& project){
    try{
        auto values=nlohmann::json::object();const auto path=projectSettingsPath(project);
        if(std::filesystem::exists(path)){
            if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("Project settings exceed their budget");
            std::ifstream input(path);auto document=nlohmann::json::parse(input);
            if(document.value("schemaVersion",0)!=1||document.size()!=2||!document.at("values").is_object())throw std::invalid_argument("Unsupported project settings document");
            values=document.at("values");
            for(const auto& entry:values.items())if(!projectSetting(entry.key()))throw std::invalid_argument("Personal setting in project configuration: "+entry.key());
        }
        return registry.replaceLayer(values,SettingSource::Project);
    }catch(const std::exception& error){return {false,error.what()};}
}
}
