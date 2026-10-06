#include "editor/ai/ProposalController.hpp"
#include "editor/EditorSession.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Fixed: IModelTransport{
    std::vector<std::string> responses;
    unsigned calls=0,cancellations=0;
    std::shared_future<ModelResponse> request(const ModelRequest&) override {
        std::promise<ModelResponse> promise;
        promise.set_value({true,responses.at(std::min<std::size_t>(calls++,responses.size()-1)),{},"json"});return promise.get_future().share();
    }
    void cancel(const std::string&) override {++cancellations;}
};
std::string artifact(const std::string& id){return Json{{"schemaVersion",1},{"operations",Json::array({{{"command","node.create"},{"parameters",{{"id",id}}}}})}}.dump();}
void finish(ProposalController& controller){for(unsigned i=0;i<5&&controller.state()==ProposalState::Generating;++i)controller.poll();}
int main(){try{
    ProposalBudget invalidHistory;invalidHistory.historyBytes=1;
    bool rejectedHistory=false;try{invalidHistory.validate();}catch(const std::invalid_argument&){rejectedHistory=true;}
    require(rejectedHistory,"History budget must include its empty JSON envelope");
    auto context=std::make_shared<EditorContext>(SceneDocument::fromAsset("test.gltf"),"candidate.azscene");
    EditorSession session(context);auto fixed=std::make_shared<Fixed>();ModelClient client(fixed);
    ProposalController controller(*context,session.edits(),session.generators(),client);
    auto initial=context->documentContent();fixed->responses={artifact("generated")};
    require(controller.generate({"run-1","scene","document","Create a node"}),"A valid request must start");
    require(!controller.generate({"run-2","scene","document","Concurrent"}),"The same document must reject concurrent generation");
    finish(controller);require(controller.state()==ProposalState::Ready&&context->documentContent()==initial,"Preview must retain the authored document");
    require(controller.report().at("diff").size()>0,"Preview must expose a semantic difference");
    require(static_cast<bool>(controller.apply(session.edits())),"Candidate must apply through production batch editing");
    require(session.edits().undo().status==EditStatus::Applied&&context->documentContent()==initial,"One undo must restore a generated batch");
    fixed->responses={artifact("after-repair")};fixed->calls=0;
    fixed->responses.insert(fixed->responses.begin(),"{invalid");
    require(controller.generate({"repair","scene","document","Repair candidate"}),"Repair request must start");finish(controller);
    require(controller.state()==ProposalState::Ready&&controller.report().at("repairCount")==1&&fixed->calls==2,"Exactly one repair may produce a valid candidate");
    controller.reject();require(controller.state()==ProposalState::Rejected&&context->documentContent()==initial,"Rejection must keep document state");
    fixed->responses={"{invalid"};fixed->calls=0;
    require(controller.generate({"exhaust","scene","document","Invalid"}),"Invalid-response request must start");finish(controller);
    require(controller.state()==ProposalState::Error&&fixed->calls==2,"Repair exhaustion must stop after two calls");
    fixed->responses={artifact("stale")};fixed->calls=0;
    require(controller.generate({"stale","scene","document","Create"}),"Stale test must start");finish(controller);
    require(static_cast<bool>(session.edit("node.create",{{"id","manual"}})),"Manual editing remains available");
    const auto manual=context->documentContent();
    controller.poll();require(controller.state()==ProposalState::Stale,"Frame polling must mark a ready proposal stale after manual editing");
    require(controller.apply(session.edits()).status==EditStatus::Stale&&context->documentContent()==manual,"Manual edits must invalidate the proposal");
    require(controller.generate({"cancel","scene","document","Create"}),"Cancellation test must start");controller.cancel();controller.poll();
    require(controller.state()==ProposalState::Cancelled&&fixed->cancellations==1,"Cancel must terminate the proposal and transport request");
    ProposalController competing(*context,session.edits(),session.generators(),client);
    require(controller.generate({"leased","scene","document","Create"}),"Owner request must acquire the document lease");
    require(!competing.generate({"competing","scene","document","Create"}),"A second controller must not generate for the leased document");
    controller.cancel();require(competing.generate({"released","scene","document","Create"}),"Cancellation must release the document lease");
    competing.cancel();
    bool wrongThread=false;std::thread worker([&]{try{controller.poll();}catch(const std::logic_error&){wrongThread=true;}});worker.join();
    require(wrongThread,"Proposal polling must enforce document thread ownership");
    require(controller.generate({"undo-stale","scene","document","Create"}),"Undo invalidation request starts");finish(controller);
    require(session.edits().undo().status==EditStatus::Applied,"History must remain accessible with a ready proposal");
    const auto undone=context->documentContent();controller.poll();
    require(controller.state()==ProposalState::Stale&&controller.apply(session.edits()).status==EditStatus::Stale&&context->documentContent()==undone,
            "Undo must invalidate a candidate without applying its content");
    ProposalBudget noRepair;noRepair.repairs=0;controller.configureBudget(noRepair);fixed->calls=0;fixed->responses={"{invalid"};
    require(controller.generate({"no-repair","scene","document","Create"}),"Zero repair budget starts");finish(controller);
    require(controller.state()==ProposalState::Error&&fixed->calls==1,"Zero repair budget must make only one transport request");
    fixed->responses={artifact("within-budget")};controller.configureBudget(ProposalBudget{});
    ModelClient missing;ProposalController disabled(*context,session.edits(),session.generators(),missing);
    require(!disabled.generate({"none","scene","document","Create"})&&static_cast<bool>(session.edit("node.create",{{"id","without-ai"}})),"Unavailable AI must preserve manual operations");
    require(!session.modelAvailable(),"A new editor session must leave model assistance disabled");
    require(session.settings().set("ai.enabled",true,SettingSource::CommandLine).passed,"Optional service enablement must be configurable");
    session.settings().applyPending();session.configureModel(fixed);
    require(session.modelAvailable(),"Explicit model assembly must expose the optional service");
    const auto description=session.edit("ai.describe").value;
    require(description.at("domains").size()==2&&description.at("domains").at(0).contains("schema"),
            "Production discovery must describe registered domains and their candidate schemas");
    auto hostRequest=session.edit("ai.generate",{{"runId","host"},{"domain","scene"},{"target","document"},{"instruction","Create a node"}});
    require(static_cast<bool>(hostRequest),"Session production operation must start model generation");
    session.pollModel();require(session.proposalReport().at("state")=="Ready","Session frame polling must produce its proposal");
    require(static_cast<bool>(session.edit("ai.reject")),"Production rejection must be available");
    for(int i=0;i<35;++i){require(controller.generate({"history-"+std::to_string(i),"scene","document","Create"}),"History request starts");finish(controller);controller.reject();}
    require(controller.report().at("historyCount").get<unsigned>()<=24&&controller.report().at("historyBytes").get<unsigned>()<=128*1024,"History must retain its bounded contract");
    std::cout<<"Proposal preview, repair, cancellation, stale rejection, transaction and history passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
