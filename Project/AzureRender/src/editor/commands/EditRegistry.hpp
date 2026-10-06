#pragma once
#include "editor/commands/EditContracts.hpp"
#include <functional>
#include <map>
namespace azurerender {
class EditorContext;
class EditorSession;
class EditRegistry {
public:
    using Handler=std::function<nlohmann::json(EditorContext&,const nlohmann::json&)>;
    struct Entry { EditDescriptor descriptor; Handler handler; };
    void add(EditDescriptor descriptor,Handler handler);
    const Entry* find(const std::string& id) const;
    nlohmann::json describe() const;
    static void validate(const nlohmann::json& value,const nlohmann::json& schema);
private:
    std::map<std::string,Entry> entries_;
};
EditRegistry editorOperations(EditorSession& session);
void registerProposalOperations(EditRegistry& registry,EditorSession& session);
}
