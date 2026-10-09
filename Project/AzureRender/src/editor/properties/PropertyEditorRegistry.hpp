#pragma once
#include "editor/EditorContext.hpp"
#include "editor/PanelContext.hpp"
#include "reflection/Registry.hpp"
#include <optional>
namespace azurerender {
struct PropertyEditEvent {
    nlohmann::json value;
    bool began=false,changed=false,ended=false,cancelled=false;
    std::optional<int> axis;
};
class PropertyEditorRegistry final {
public:
    static nlohmann::json selection(const EditorContext& context,const std::vector<std::string>& nodes,const std::string& type);
    static nlohmann::json selection(const PanelDocumentView& context,const std::vector<std::string>& nodes,const std::string& type);
    static const reflection::Property& field(const std::string& type,const std::string& name);
    // Called on a transaction candidate. The caller owns atomic publication.
    static void apply(EditorContext& candidate,const nlohmann::json& parameters);
};
}
