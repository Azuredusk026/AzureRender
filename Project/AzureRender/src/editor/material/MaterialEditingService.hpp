#pragma once
#include "runtime/MaterialDocument.hpp"
#include "editor/content/ContentOperationService.hpp"
#include "editor/EditorContext.hpp"
namespace azurerender {
class MaterialEditingService {
public:
    nlohmann::json execute(EditorContext&,ContentOperationService&,const std::string&,const nlohmann::json&);
    void clear(){history_.clear();}
    static nlohmann::json slots(const EditorContext&,const std::string&);
    static nlohmann::json reimportSlots(const EditorContext&,const std::string& asset);
    static void assign(EditorContext&,const std::vector<std::string>&,const std::string&,const std::string&);
private:
    struct History {std::vector<nlohmann::json> undo,redo;nlohmann::json current;};
    std::map<std::string,History> history_;
};
}
