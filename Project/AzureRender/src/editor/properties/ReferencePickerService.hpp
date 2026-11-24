#pragma once
#include "editor/properties/PropertyEditorRegistry.hpp"
#include "editor/commands/DocumentVersion.hpp"
namespace azurerender {
struct ReferencePickRequest {
    DocumentVersion version;
    std::vector<std::string> owners;
    std::string type,field,kind;
    std::vector<std::string> assetTypes;
};
class ReferencePickerService final {
public:
    bool active() const noexcept {return request_.has_value();}
    const std::optional<ReferencePickRequest>& request() const {return request_;}
    void begin(const EditorContext& context,const nlohmann::json& args);
    void deliver(EditorContext& candidate,const DocumentVersion& version,const std::string& kind,const std::string& id);
    void cancel() noexcept {request_.reset();}
private:
    std::optional<ReferencePickRequest> request_;
};
}
