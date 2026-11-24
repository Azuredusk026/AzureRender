#pragma once
#include "editor/commands/EditRegistry.hpp"
namespace azurerender {
class EditService {
public:
    using Enabled=std::function<bool(const EditDescriptor&)>;
    EditService(EditorContext& context,EditRegistry registry,Enabled enabled={});
    DocumentVersion version() const;
    nlohmann::json describe() const { return registry_.describe(); }
    EditRegistry& registry() { return registry_; }
    EditResult execute(const EditRequest& request);
    EditResult executeBatch(const std::vector<EditRequest>& requests);
    EditResult current(const std::string& command,nlohmann::json parameters=nlohmann::json::object(),std::string mergeKey={});
    EditResult undo() { return current("history.undo"); }
    EditResult redo() { return current("history.redo"); }
private:
    EditorContext& context_;
    EditRegistry registry_;
    Enabled enabled_;
    std::uint64_t sequence_=0;
    EditResult run(const std::vector<EditRequest>& requests,bool batch);
};
}
