#pragma once
#include "editor/commands/EditService.hpp"
#include "ai/ModelClient.hpp"
#include <thread>
namespace azurerender {
class GeneratorRegistry;
enum class ProposalState {Idle,Generating,Validating,Ready,Stale,Applied,Rejected,Cancelled,Error};
const char* proposalStateName(ProposalState state);
struct ProposalRequest {std::string runId,domain,targetId,instruction,profile="content";};
struct ProposalCandidate {
    std::vector<EditRequest> operations;
    nlohmann::json diff=nlohmann::json::array();
    bool transactional=true;
};
struct ProposalValidationContext {
    EditorContext& document;
    EditService& edits;
    const GeneratorRegistry& generators;
    DocumentVersion baseVersion;
    std::string runId,targetId;
};
class IProposalAdapter {
public:
    virtual ~IProposalAdapter()=default;
    virtual std::string id() const=0;
    virtual nlohmann::json schema() const=0;
    virtual nlohmann::json snapshot(const ProposalValidationContext&) const=0;
    virtual ProposalCandidate validate(const std::string& source,const ProposalValidationContext&) const=0;
};
}
