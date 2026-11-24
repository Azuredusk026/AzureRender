#pragma once
#include "editor/EditorContext.hpp"
#include <functional>
namespace azurerender {
class EditRegistry;
class EditorSession;
class ScriptAuthoringRegistry {
public:
    struct Backend {std::string id,label,extension;unsigned version=1;std::function<std::string(const std::string&)> source;};
    void add(Backend);
    nlohmann::json describe() const;
    nlohmann::json create(EditorContext&,const std::string& backend,const std::string& reference) const;
    bool contains(const std::string& id) const{return backends_.count(id)!=0;}
    static ScriptAuthoringRegistry builtins();
private:
    std::map<std::string,Backend> backends_;
};
void registerScriptAuthoringOperations(EditRegistry&,EditorSession&);
}
