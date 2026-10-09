#pragma once
#include "editor/EditorContext.hpp"
namespace azurerender {
class EditorSession;
class EditRegistry;
class PrefabAuthoringService {
public:
    PrefabAuthoringService();
    ~PrefabAuthoringService();
    nlohmann::json create(EditorSession&,const std::vector<std::string>& nodes,const std::string& path);
    nlohmann::json overrides(EditorContext&,const std::string& instance) const;
    void revert(EditorContext&,const std::string& instance,const nlohmann::json& fields);
    nlohmann::json apply(EditorSession&,const std::string& instance,const nlohmann::json& fields,const std::string& baseFingerprint);
    void undoApply(EditorSession&);
    void redoApply(EditorSession&);
    void clear();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void restoreApplication(EditorSession&,bool forward);
    static void adopt(EditorContext&,const nlohmann::json&,const std::map<std::string,nlohmann::json>& sources={});
};
void registerPrefabAuthoringOperations(EditRegistry&,EditorSession&);
}
