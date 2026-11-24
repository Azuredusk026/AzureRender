#pragma once
#include "ProposalContracts.hpp"
#include "ValidationPolicy.hpp"
#include <map>
#include <chrono>
namespace azurerender {
class ProposalController {
public:
    ProposalController(EditorContext&,EditService&,const GeneratorRegistry&,ModelClient&,ProposalBudget={});
    ~ProposalController();
    ProposalController(const ProposalController&)=delete;
    ProposalController& operator=(const ProposalController&)=delete;
    void addAdapter(std::shared_ptr<IProposalAdapter> adapter);
    void configureBudget(ProposalBudget budget);
    bool generate(const ProposalRequest& request);
    void poll();
    void cancel();
    void reject();
    bool validate();
    EditResult apply(EditService& edits);
    ProposalState state() const;
    nlohmann::json report() const;
    nlohmann::json describe() const;
private:
    EditorContext& document_;
    EditService& edits_;
    const GeneratorRegistry& generators_;
    ModelClient& client_;
    ProposalBudget budget_;
    std::thread::id owner_=std::this_thread::get_id();
    std::map<std::string,std::shared_ptr<IProposalAdapter>> adapters_;
    ProposalState state_=ProposalState::Idle;
    ProposalRequest request_;
    DocumentVersion base_;
    ModelRequest envelope_;
    std::shared_future<ModelResponse> pending_;
    ModelResponse response_;
    ProposalCandidate candidate_;
    nlohmann::json diagnostics_=nlohmann::json::array(),history_=nlohmann::json::array();
    unsigned repairs_=0;
    bool lease_=false;
    std::chrono::steady_clock::time_point started_;
    void checkThread() const;
    void release();
    ProposalValidationContext validationContext() const;
    void remember();
};
std::vector<std::shared_ptr<IProposalAdapter>> builtinProposalAdapters();
}
