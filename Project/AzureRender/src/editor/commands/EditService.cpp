#include "editor/commands/EditService.hpp"
#include "editor/commands/EditTransaction.hpp"
#include <set>
namespace azurerender {
EditService::EditService(EditorContext& context,EditRegistry registry,Enabled enabled)
    :context_(context),registry_(std::move(registry)),enabled_(std::move(enabled)) {}
DocumentVersion EditService::version() const { return context_.documentVersion(); }
EditResult EditService::current(const std::string& command,nlohmann::json parameters,std::string mergeKey) {
    return execute({"frontend-"+std::to_string(++sequence_),command,std::move(parameters),version(),std::move(mergeKey)});
}
EditResult EditService::execute(const EditRequest& request) { return run({request},false); }
EditResult EditService::executeBatch(const std::vector<EditRequest>& requests) { return run(requests,true); }
EditResult EditService::run(const std::vector<EditRequest>& requests,bool batch) {
    EditResult result;result.version=version();
    try {
        if(requests.empty()||requests.size()>128)throw EditRejection("Edit request count exceeds its budget");
        std::vector<const EditRegistry::Entry*> entries;std::set<std::string> ids;
        for(const auto& request:requests) {
            if(request.baseVersion!=result.version) { result.status=EditStatus::Stale;
                result.diagnostics.push_back({{"code","StaleVersion"},{"requestId",request.requestId}});return result; }
            if(request.requestId.empty()||!ids.insert(request.requestId).second)throw EditRejection("Invalid request identity");
            const auto* entry=registry_.find(request.commandId);
            if(!entry)throw EditRejection("Unknown operation: "+request.commandId);
            if(enabled_&&!enabled_(entry->descriptor))throw EditRejection("Operation requires an idle edit session");
            if(batch&&!entry->descriptor.transactional)throw EditRejection("Operation has external or session effects");
            EditRegistry::validate(request.parameters,entry->descriptor.parameters);entries.push_back(entry);
        }
        const auto before=context_.documentContent();
        if(batch||entries.front()->descriptor.transactional) {
            EditTransaction transaction(context_);
            auto values=nlohmann::json::array();
            for(std::size_t index=0;index<requests.size();++index) {
                const auto previous=transaction.candidate().documentContent();
                values.push_back(entries[index]->handler(transaction.candidate(),requests[index].parameters));
                if(!entries[index]->descriptor.modifiesDocument&&transaction.candidate().documentContent()!=previous)
                    throw EditRejection("Read-only operation modified the document");
            }
            transaction.commit(batch?std::string():requests.front().mergeKey);
            result.value=batch?values:values.front();
        }else {
            auto state=context_.snapshot();auto undo=context_.undoStack_,redo=context_.redoStack_;
            const auto dirty=context_.dirty_;const auto revision=context_.revision_;
            const auto preview=context_.animationPreview_;const auto writes=context_.resourceWriteTimes_;
            const auto mergeKey=context_.mergeKey_;const auto mergeRevision=context_.mergeRevision_;
            const auto mergeSelection=context_.mergeSelection_;const auto importSummary=context_.importSummary_;
            try {
                if(entries.front()->descriptor.modifiesDocument||entries.front()->descriptor.requiresIdle)context_.closeEditMerge();
                result.value=entries.front()->handler(context_,requests.front().parameters);
                if(!entries.front()->descriptor.modifiesDocument&&context_.documentContent()!=before)
                    throw EditRejection("Read-only operation modified the document");
            }catch(...) {
                context_.restore(std::move(state));context_.undoStack_=std::move(undo);context_.redoStack_=std::move(redo);
                context_.dirty_=dirty;context_.revision_=revision;context_.animationPreview_=preview;context_.resourceWriteTimes_=writes;
                context_.mergeKey_=mergeKey;context_.mergeRevision_=mergeRevision;context_.mergeSelection_=mergeSelection;
                context_.importSummary_=importSummary;throw;
            }
        }
        result.status=EditStatus::Applied;result.version=version();result.diff=nlohmann::json::diff(before,context_.documentContent());
    }catch(const EditRejection& error) {
        result.status=EditStatus::Rejected;result.diagnostics.push_back({{"code","RejectedOperation"},{"message",error.what()}});
    }catch(const std::exception& error) {
        result.status=EditStatus::Failed;result.diagnostics.push_back({{"code","OperationFailed"},{"message",error.what()}});
    }
    return result;
}
}
