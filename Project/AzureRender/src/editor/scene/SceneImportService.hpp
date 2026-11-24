#pragma once
#include "assets/scene/SceneExchange.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
class EditorContext;class EditorSession;class EditRegistry;
class SceneImportService {
public:
 // Called on a transaction candidate. The caller owns commit and rollback.
 static nlohmann::json apply(EditorContext& context,const SceneImportProposal& proposal);
 // Uses the editor task service: worker parsing, cancellation, main-thread commit.
 static std::string start(EditorSession& session,const std::filesystem::path& source);
};
void registerSceneImportOperations(EditRegistry& registry,EditorSession& session);
}
