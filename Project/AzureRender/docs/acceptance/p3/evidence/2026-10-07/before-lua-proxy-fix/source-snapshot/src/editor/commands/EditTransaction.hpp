#pragma once
#include "editor/EditorContext.hpp"
namespace azurerender {
class EditTransaction {
public:
    explicit EditTransaction(EditorContext& document);
    EditorContext& candidate() { return *candidate_; }
    void commit(const std::string& mergeKey);
private:
    EditorContext& document_;
    std::unique_ptr<EditorContext> candidate_;
};
}
