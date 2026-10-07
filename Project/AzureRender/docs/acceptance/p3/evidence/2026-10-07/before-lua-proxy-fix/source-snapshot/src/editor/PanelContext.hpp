#pragma once
#include "SelectionService.hpp"
#include "PanelDocumentView.hpp"
#include "runtime/SceneDocument.hpp"
namespace azurerender {
class EditService;class AssetDatabase;
class PanelContext {
public:
    PanelContext(const EditorContext& document,SelectionService& selection,EditService& edits):view_(document),selection_(selection),edits_(edits){}
    const SceneDocument& document() const noexcept{return view_.scene();}
    const PanelDocumentView& view() const noexcept{return view_;}
    SelectionService& selection() const noexcept{return selection_;}
    EditService& edits() const noexcept{return edits_;}
private:
    PanelDocumentView view_;SelectionService& selection_;EditService& edits_;
};
}
