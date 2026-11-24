#include "ProposalController.hpp"
#include "editor/EditorContext.hpp"
#include "assets/GeneratorRegistry.hpp"
#include <mutex>
#include <set>
#include <algorithm>
namespace azurerender {
namespace {
std::mutex leasesMutex;
std::map<std::string,const ProposalController*> leases;
}
const char* proposalStateName(ProposalState state){
    switch(state){case ProposalState::Idle:return "Idle";case ProposalState::Generating:return "Generating";
    case ProposalState::Validating:return "Validating";case ProposalState::Ready:return "Ready";case ProposalState::Stale:return "Stale";
    case ProposalState::Applied:return "Applied";case ProposalState::Rejected:return "Rejected";case ProposalState::Cancelled:return "Cancelled";case ProposalState::Error:return "Error";}
    throw std::invalid_argument("Unknown proposal state");
}
ProposalController::ProposalController(EditorContext& document,EditService& edits,const GeneratorRegistry& generators,ModelClient& client,ProposalBudget budget)
    :document_(document),edits_(edits),generators_(generators),client_(client),budget_(budget){
    budget_.validate();for(auto& adapter:builtinProposalAdapters())addAdapter(std::move(adapter));
}
ProposalController::~ProposalController(){if(state_==ProposalState::Generating)client_.cancel(envelope_.runId);release();}
void ProposalController::checkThread() const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Proposals require the document owner thread");}
void ProposalController::release(){if(!lease_)return;std::lock_guard<std::mutex> guard(leasesMutex);const auto found=leases.find(base_.documentId);if(found!=leases.end()&&found->second==this)leases.erase(found);lease_=false;}
void ProposalController::addAdapter(std::shared_ptr<IProposalAdapter> adapter){
    checkThread();if(!adapter||adapter->id().empty()||adapter->id().size()>128||adapters_.size()>=64||state_==ProposalState::Generating)
        throw std::invalid_argument("Invalid proposal adapter registration");
    if(!adapters_.emplace(adapter->id(),std::move(adapter)).second)throw std::invalid_argument("Duplicate proposal adapter");
}
void ProposalController::configureBudget(ProposalBudget budget){checkThread();if(state_==ProposalState::Generating||state_==ProposalState::Validating)throw std::logic_error("Budget changes require an idle proposal");budget.validate();budget_=budget;while(history_.size()>budget_.historyCount||history_.dump().size()>budget_.historyBytes)history_.erase(history_.begin());}
ProposalValidationContext ProposalController::validationContext() const{return {document_,edits_,generators_,base_,request_.runId,request_.targetId};}
bool ProposalController::generate(const ProposalRequest& request){
    checkThread();if(state_==ProposalState::Generating||state_==ProposalState::Validating)return false;
    release();request_=request;base_=edits_.version();repairs_=0;response_={};candidate_={};diagnostics_=nlohmann::json::array();
    try{
        if(!client_.available())throw std::invalid_argument("Optional model service unavailable");
        if(request.runId.empty()||request.runId.size()>128||request.instruction.empty()||request.instruction.size()>budget_.sourceBytes)
            throw std::invalid_argument("Proposal identity or instruction exceeds budget");
        const auto adapter=adapters_.at(request.domain);
        if(request.targetId!="document"){
            const auto& nodes=document_.scene().nodes;
            if(std::none_of(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==request.targetId;}))throw std::invalid_argument("Unknown proposal target");
        }
        envelope_={};envelope_.runId=request.runId;envelope_.profile=request.profile;envelope_.schema=adapter->schema();envelope_.timeoutMs=budget_.timeoutMs;envelope_.history=history_;
        envelope_.prompt=nlohmann::json{{"instruction",request.instruction},{"domain",request.domain},{"target",request.targetId},
            {"baseVersion",base_.describe()},{"snapshot",adapter->snapshot(validationContext())}}.dump();
        validateModelRequest(envelope_);
        if(envelope_.prompt.size()>budget_.sourceBytes)throw std::invalid_argument("Proposal snapshot exceeds source budget");
        {std::lock_guard<std::mutex> guard(leasesMutex);if(!leases.emplace(base_.documentId,this).second)throw std::invalid_argument("Document already owns a generation request");lease_=true;}
        started_=std::chrono::steady_clock::now();state_=ProposalState::Generating;pending_=client_.request(envelope_);return true;
    }catch(const std::exception& error){state_=ProposalState::Error;diagnostics_.push_back({{"code","RequestRejected"},{"message",std::string(error.what()).substr(0,4096)}});release();return false;}
}
bool ProposalController::validate(){
    checkThread();if(!response_.passed||(state_!=ProposalState::Generating&&state_!=ProposalState::Ready&&state_!=ProposalState::Validating))return false;
    state_=ProposalState::Validating;
    try{
        if(edits_.version()!=base_){state_=ProposalState::Stale;release();remember();return false;}
        if(response_.content.size()>budget_.sourceBytes)throw std::invalid_argument("Proposal response exceeds source budget");
        auto candidate=adapters_.at(request_.domain)->validate(response_.content,validationContext());
        if(candidate.operations.empty()||candidate.operations.size()>budget_.operations)throw std::invalid_argument("Proposal operations exceed budget");
        candidate_=std::move(candidate);state_=ProposalState::Ready;release();return true;
    }catch(const std::exception& error){diagnostics_.push_back({{"code","InvalidProposal"},{"message",std::string(error.what()).substr(0,4096)}});return false;}
}
void ProposalController::poll(){
    checkThread();
    if(state_==ProposalState::Ready&&edits_.version()!=base_){state_=ProposalState::Stale;release();remember();return;}
    if(state_!=ProposalState::Generating)return;
    if(std::chrono::steady_clock::now()-started_>=std::chrono::milliseconds(budget_.timeoutMs)){
        client_.cancel(envelope_.runId);state_=ProposalState::Error;diagnostics_.push_back({{"code","Timeout"},{"message","Proposal deadline exceeded"}});release();remember();return;
    }
    if(edits_.version()!=base_){client_.cancel(envelope_.runId);state_=ProposalState::Stale;release();remember();return;}
    if(pending_.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
    try{
        response_=pending_.get();
        if(!response_.passed)throw std::runtime_error(response_.diagnostic);
        if(validate()||state_==ProposalState::Stale)return;
        if(repairs_<budget_.repairs){
            ++repairs_;envelope_.runId=request_.runId+"-repair-"+std::to_string(repairs_);
            const auto remaining=std::chrono::milliseconds(budget_.timeoutMs)-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started_);
            if(remaining.count()<1)throw std::runtime_error("Proposal repair deadline exhausted");
            envelope_.timeoutMs=static_cast<std::uint32_t>(remaining.count());
            const auto snapshot=adapters_.at(request_.domain)->snapshot(validationContext());
            envelope_.prompt=nlohmann::json{{"instruction",request_.instruction},{"snapshot",snapshot},{"diagnostics",diagnostics_},
                {"repairSourcePrefix",response_.content.substr(0,65536)},{"sourceTruncated",response_.content.size()>65536}}.dump();
            validateModelRequest(envelope_);
            if(envelope_.prompt.size()>budget_.sourceBytes)throw std::runtime_error("Repair prompt exceeds source budget");
            state_=ProposalState::Generating;pending_=client_.request(envelope_);return;
        }
        throw std::runtime_error("Proposal repair budget exhausted");
    }catch(const std::exception& error){state_=ProposalState::Error;diagnostics_.push_back({{"code","GenerationFailed"},{"message",std::string(error.what()).substr(0,4096)}});release();remember();}
}
void ProposalController::remember(){
    const auto text=nlohmann::json{{"runId",request_.runId},{"domain",request_.domain},{"target",request_.targetId},
        {"instruction",request_.instruction.substr(0,4096)},{"status",proposalStateName(state_)},{"repairCount",repairs_}}.dump();
    history_.push_back({{"role","assistant"},{"content",text}});
    while(history_.size()>budget_.historyCount||history_.dump().size()>budget_.historyBytes)history_.erase(history_.begin());
}
void ProposalController::cancel(){checkThread();if(state_==ProposalState::Generating)client_.cancel(envelope_.runId);state_=ProposalState::Cancelled;candidate_={};release();remember();}
void ProposalController::reject(){checkThread();if(state_==ProposalState::Generating)client_.cancel(envelope_.runId);state_=ProposalState::Rejected;candidate_={};release();remember();}
EditResult ProposalController::apply(EditService& edits){
    checkThread();EditResult result;result.version=edits.version();
    if(result.version!=base_){state_=ProposalState::Stale;result.status=EditStatus::Stale;result.diagnostics.push_back({{"code","StaleProposal"}});release();remember();return result;}
    if(state_!=ProposalState::Ready){result.diagnostics.push_back({{"code","ProposalNotReady"}});return result;}
    try{
        auto refreshed=adapters_.at(request_.domain)->validate(response_.content,validationContext());
        if(refreshed.diff!=candidate_.diff){state_=ProposalState::Stale;result.status=EditStatus::Stale;result.diagnostics.push_back({{"code","ChangedProposalInputs"}});release();remember();return result;}
        candidate_=std::move(refreshed);
        result=candidate_.transactional?edits.executeBatch(candidate_.operations):edits.execute(candidate_.operations.at(0));
        state_=result?ProposalState::Applied:result.status==EditStatus::Stale?ProposalState::Stale:ProposalState::Error;
        if(!result)for(const auto& row:result.diagnostics)diagnostics_.push_back(row);
    }catch(const std::exception& error){state_=ProposalState::Error;result.status=EditStatus::Rejected;result.diagnostics.push_back({{"code","InvalidProposal"},{"message",std::string(error.what()).substr(0,4096)}});}
    release();remember();return result;
}
ProposalState ProposalController::state() const{checkThread();return state_;}
nlohmann::json ProposalController::describe() const{
    checkThread();auto domains=nlohmann::json::array();
    for(const auto& [id,adapter]:adapters_)domains.push_back({{"id",id},{"schema",adapter->schema()}});
    return {{"schemaVersion",1},{"available",client_.available()},{"domains",domains},{"proposal",report()}};
}
nlohmann::json ProposalController::report() const{
    checkThread();auto operations=nlohmann::json::array();
    for(const auto& operation:candidate_.operations)operations.push_back({{"command",operation.commandId},{"parameters",operation.parameters}});
    return {{"schemaVersion",1},{"state",proposalStateName(state_)},{"runId",request_.runId},{"domain",request_.domain},{"target",request_.targetId},
        {"baseVersion",base_.describe()},{"repairCount",repairs_},{"structuredMode",response_.structuredMode},{"diff",candidate_.diff},
        {"operations",operations},{"diagnostics",diagnostics_},{"historyCount",history_.size()},{"historyBytes",history_.dump().size()},
        {"sourceHash",generationHash(response_.content)},{"transactional",candidate_.transactional}};
}
}
