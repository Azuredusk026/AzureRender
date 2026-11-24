#pragma once
#include "editor/commands/EditTransaction.hpp"
namespace azurerender {
class DocumentBatchTransaction {
public:
    explicit DocumentBatchTransaction(const std::vector<EditorContext*>& documents);
    ~DocumentBatchTransaction();
    EditorContext& candidate(EditorContext& document);
    void commit(const std::function<void()>& publish,const std::function<void()>& rollback);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
