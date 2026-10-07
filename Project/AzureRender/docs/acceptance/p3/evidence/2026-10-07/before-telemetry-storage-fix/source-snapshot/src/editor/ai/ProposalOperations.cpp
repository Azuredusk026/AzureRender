#include "editor/EditorSession.hpp"
namespace azurerender {
void registerProposalOperations(EditRegistry& registry,EditorSession& session){
    using Json=nlohmann::json;const Json text={{"type","string"}},empty={{"type","object"},{"properties",Json::object()}};
    const Json schema={{"type","object"},{"properties",{{"runId",text},{"domain",text},{"target",text},{"instruction",{{"type","string"},{"maxLength",2097152}}}}},
        {"required",{"runId","domain","target","instruction"}}};
    registry.add({"ai.generate",1,schema,false,false,true},[&session](EditorContext&,const Json& parameters)->Json{
        auto& settings=session.settings();ProposalBudget budget;
        budget.operations=settings.get("ai.maxOperations").get<std::size_t>();budget.sourceBytes=settings.get("ai.maxSourceBytes").get<std::size_t>();
        budget.historyCount=settings.get("ai.maxHistoryMessages").get<std::size_t>();budget.historyBytes=settings.get("ai.maxHistoryBytes").get<std::size_t>();
        budget.repairs=settings.get("ai.maxRepairs").get<unsigned>();budget.timeoutMs=settings.get("ai.timeoutMs").get<std::uint32_t>();
        auto& controller=session.proposals();controller.configureBudget(budget);
        if(!controller.generate({parameters.at("runId").get<std::string>(),parameters.at("domain").get<std::string>(),parameters.at("target").get<std::string>(),
            parameters.at("instruction").get<std::string>(),settings.get("ai.profile").get<std::string>()}))throw EditRejection(controller.report().dump());
        return controller.report();
    });
    registry.add({"ai.describe",1,empty,false,false,false},[&session](EditorContext&,const Json&)->Json{
        return session.modelAvailable()?session.proposals().describe():Json{{"schemaVersion",1},{"available",false},{"domains",Json::array()},{"proposal",session.proposalReport()}};
    });
    registry.add({"ai.cancel",1,empty,false,false,false},[&session](EditorContext&,const Json&)->Json{session.proposals().cancel();return session.proposalReport();});
    registry.add({"ai.reject",1,empty,false,false,false},[&session](EditorContext&,const Json&)->Json{session.proposals().reject();return session.proposalReport();});
    registry.add({"ai.apply",1,empty,true,false,true},[&session](EditorContext&,const Json&)->Json{
        const auto result=session.proposals().apply(session.edits());if(!result)throw EditRejection(result.diagnostics.dump());return session.proposalReport();
    });
}
}
