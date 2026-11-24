#include "DocumentBatchTransaction.hpp"
#include <set>
namespace azurerender {
struct DocumentBatchTransaction::Impl {
    struct Entry {
        EditorContext* document;
        EditTransaction transaction;
        EditorContext::Snapshot before;
        std::vector<EditorContext::Snapshot> undo,redo;
        DocumentVersion version;
        bool dirty;
        std::optional<NodeAnimationFrame> animation;
        std::string mergeKey,saved;
        std::uint64_t mergeRevision;
        std::vector<std::size_t> mergeSelection;
        std::unique_ptr<AssetDatabase> assets;
        explicit Entry(EditorContext& d):document(&d),transaction(d),before(d.snapshot()),undo(d.undoStack_),redo(d.redoStack_),version(d.documentVersion()),dirty(d.dirty_),animation(d.animationPreview_),mergeKey(d.mergeKey_),saved(d.savedFingerprint_),mergeRevision(d.mergeRevision_),mergeSelection(d.mergeSelection_),assets(d.assets_?std::make_unique<AssetDatabase>(*d.assets_):nullptr){}
        void restore(){auto& d=*document;d.restore(before);d.undoStack_=undo;d.redoStack_=redo;d.revision_=version.revision;d.dirty_=dirty;d.animationPreview_=animation;d.mergeKey_=mergeKey;d.savedFingerprint_=saved;d.mergeRevision_=mergeRevision;d.mergeSelection_=mergeSelection;if(assets)d.assets_=std::make_unique<AssetDatabase>(*assets);}
    };
    std::vector<std::unique_ptr<Entry>> entries;
    bool committed=false;
};
DocumentBatchTransaction::DocumentBatchTransaction(const std::vector<EditorContext*>& documents):impl_(std::make_unique<Impl>()){
    if(documents.empty()||documents.size()>32)throw std::invalid_argument("Document transaction requires 1 to 32 targets");std::set<EditorContext*> unique;
    for(auto* document:documents){if(!document||!unique.insert(document).second)throw std::invalid_argument("Document transaction requires unique targets");impl_->entries.push_back(std::make_unique<Impl::Entry>(*document));}
}
DocumentBatchTransaction::~DocumentBatchTransaction()=default;
EditorContext& DocumentBatchTransaction::candidate(EditorContext& document){for(auto& entry:impl_->entries)if(entry->document==&document)return entry->transaction.candidate();throw std::invalid_argument("Document is outside the transaction");}
void DocumentBatchTransaction::commit(const std::function<void()>& publish,const std::function<void()>& rollback){
    if(impl_->committed||!publish||!rollback)throw std::logic_error("Document transaction requires one recoverable publication");
    for(auto& entry:impl_->entries){if(entry->document->documentVersion()!=entry->version)throw std::runtime_error("Document changed during batch transaction");entry->transaction.validate();}
    bool published=false;
    try{published=true;publish();for(auto& entry:impl_->entries)entry->transaction.commit({});impl_->committed=true;}
    catch(...){
        const auto failure=std::current_exception();std::string recoveryErrors;
        if(published)try{rollback();}catch(const std::exception& error){recoveryErrors="Publication rollback: "+std::string(error.what());}catch(...){recoveryErrors="Publication rollback failed";}
        for(auto& entry:impl_->entries)try{entry->restore();}catch(const std::exception& error){recoveryErrors+="; document restore: "+std::string(error.what());}catch(...){recoveryErrors+="; document restore failed";}
        if(!recoveryErrors.empty()){try{std::rethrow_exception(failure);}catch(...){std::throw_with_nested(std::runtime_error(recoveryErrors));}}
        std::rethrow_exception(failure);
    }
}
}
