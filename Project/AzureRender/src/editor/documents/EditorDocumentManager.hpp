#pragma once
#include "editor/EditorContext.hpp"
#include "editor/commands/EditService.hpp"
#include "editor/SelectionService.hpp"
#include "editor/viewport/GizmoController.hpp"
#include "editor/viewport/EditorViewDescriptor.hpp"
#include "editor/animation/AnimationPreviewService.hpp"
#include <map>
namespace azurerender {
class EditorDocumentManager {
public:
    struct Document {
        std::shared_ptr<EditorContext> context;
        std::shared_ptr<EditService> edits;
        std::shared_ptr<SelectionService> selection;
        std::shared_ptr<GizmoController> gizmo;
        EditorViewDescriptor view;
        AnimationPreviewService animations;
    };
    std::string add(Document document);
    Document& at(const std::string& id);
    const Document& at(const std::string& id) const;
    std::string find(const std::filesystem::path& path) const;
    nlohmann::json describe(const std::string& active,bool versions=true) const;
    void save(const std::string& id);
    void erase(const std::string& id);
    void clear(){documents_.clear();}
    bool dirtyExcept(const std::string& id) const;
    void saveAll();
    std::size_t size() const{return documents_.size();}
private:
    std::map<std::string,Document> documents_;
};
}
