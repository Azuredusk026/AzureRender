#include "editor/ai/ProposalController.hpp"
#include "editor/EditorSession.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class Fn> void reject(Fn fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}require(failed,"An invalid domain proposal must be rejected");}
Json artifact(Json operations){return {{"schemaVersion",1},{"operations",std::move(operations)}};}
Json operation(std::string command,Json parameters){return {{"command",std::move(command)},{"parameters",std::move(parameters)}};}
int main(int argc,char** argv){
    const auto directory=std::filesystem::temp_directory_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try{
        require(argc==3,"Expected exploration and inspector projects");
        std::filesystem::create_directories(directory);
        auto document=std::make_shared<EditorContext>(SceneDocument::fromAsset("test.gltf"),directory/"test.azscene");EditorSession session(document);
        const auto adapters=builtinProposalAdapters();const auto scene=adapters.at(0),assets=adapters.at(1);
        ProposalValidationContext context{*document,session.edits(),session.generators(),session.edits().version(),"adapter","document"};
        const auto before=document->documentContent();
        const auto valid=artifact(Json::array({operation("node.create",{{"id","candidate"}}),operation("node.transform",{{"translation",{1,2,3}}})}));
        auto candidate=scene->validate(valid.dump(),context);
        require(candidate.transactional&&candidate.operations.size()==2&&candidate.diff.size()>0&&document->documentContent()==before,"Scene preview must use isolated production semantics");
        for(auto invalid:std::vector<Json>{
            artifact(Json::array({operation("node.place",{{"resource","unknown-reference"}})})),
            artifact(Json::array({operation("node.transform",{{"translation",{1e100,0,0}}})})),
            artifact(Json::array({operation("node.create",{{"id","duplicate"}}),operation("node.create",{{"id","duplicate"}})})),
            artifact(Json::array({operation("script.write",{{"path","outside.lua"},{"value","arbitrary"}})})),
            artifact(Json::array({operation("node.create",{{"id","candidate"},{"extra",1}})}))}){
            reject([&]{scene->validate(invalid.dump(),context);});require(document->documentContent()==before,"Rejected scene preview must preserve the document");
        }
        auto wrongVersion=valid;wrongVersion["schemaVersion"]=1.0;reject([&]{scene->validate(wrongVersion.dump(),context);});
        wrongVersion=valid;wrongVersion["extra"]=true;reject([&]{scene->validate(wrongVersion.dump(),context);});
        auto excess=Json::array();for(int i=0;i<129;++i)excess.push_back(operation("node.create",{{"id","item-"+std::to_string(i)}}));
        reject([&]{scene->validate(artifact(excess).dump(),context);});
        auto scoped=context;scoped.targetId=document->scene().nodes.front().id;
        reject([&]{scene->validate(valid.dump(),scoped);});
        const auto scopedArtifact=artifact(Json::array({operation("node.select",{{"id",scoped.targetId}}),operation("node.rename",{{"value","Scoped target"}})}));
        require(scene->validate(scopedArtifact.dump(),scoped).operations.size()==2,"A stable node scope must accept only its declared target");
        for(int projectIndex=0;projectIndex<2;++projectIndex){
            const auto path=directory/('a'+std::to_string(projectIndex));std::filesystem::copy(argv[projectIndex+1],path,std::filesystem::copy_options::recursive);
            auto project=EditorContext::openProject(path/"project.azureproject");EditorSession editor(project);
            ProposalValidationContext projectContext{*project,editor.edits(),editor.generators(),editor.edits().version(),"asset","document"};
            const auto generated=path/"assets/generated-ai.json";
            Json parameters={{"generator","azure.text"},{"output","assets:/generated-ai.json"},{"license","CC0 test fixture"},{"parameters",{{"source","{\"schemaVersion\":1,\"tag\":\"reusable-tool\"}"}}}};
            const auto preview=assets->validate(artifact(Json::array({operation("asset.generate",parameters)})).dump(),projectContext);
            require(!preview.transactional&&preview.operations.size()==1&&!std::filesystem::exists(generated),"Asset preview must retain all project files");
            auto bad=parameters;bad["output"]="assets:/../outside.json";reject([&]{assets->validate(artifact(Json::array({operation("asset.generate",bad)})).dump(),projectContext);});
            bad=parameters;bad["dependencies"]={"unknown-id"};reject([&]{assets->validate(artifact(Json::array({operation("asset.generate",bad)})).dump(),projectContext);});
            bad=parameters;bad["parameters"]["source"]="{broken";reject([&]{assets->validate(artifact(Json::array({operation("asset.generate",bad)})).dump(),projectContext);});
            const auto result=editor.edits().execute(preview.operations.front());
            require(result.status==EditStatus::Applied&&std::filesystem::is_regular_file(generated),"Asset application must use the production generation service");
            const auto id=editor.context().assets().idForPath("assets:/generated-ai.json");
            require(editor.context().assets().records().at(id).generation.has_value(),"Generated asset must retain its source manifest");
            projectContext.baseVersion=editor.edits().version();projectContext.runId="asset-model";
            std::string modelSource;
            for(const auto& entry:editor.context().assets().records())if(entry.second.path.extension()==".gltf"){
                std::ifstream input(entry.second.path,std::ios::binary);modelSource.assign(std::istreambuf_iterator<char>(input),{});break;
            }
            require(!modelSource.empty(),"Project fixture requires an existing embedded model");
            auto modelParameters=parameters;modelParameters["output"]="assets:/generated-ai.gltf";modelParameters["parameters"]["source"]=modelSource;
            const auto modelPreview=assets->validate(artifact(Json::array({operation("asset.generate",modelParameters)})).dump(),projectContext);
            require(!std::filesystem::exists(path/"assets/generated-ai.gltf"),"Model proposal validation must preserve the asset tree");
            require(editor.edits().execute(modelPreview.operations.front()).status==EditStatus::Applied,"Validated model proposal uses production asset installation");
            const auto modelId=editor.context().assets().idForPath("assets:/generated-ai.gltf");
            const auto beforePlacement=editor.context().documentContent();
            require(static_cast<bool>(editor.edit("node.place",{{"resource",modelId},{"id","generated-placement"}})),"Registered model identities must be placeable through the shared operation");
            require(editor.context().selectedNode()->resourceId==modelId,"Placed content binds its existing asset identity");
            require(static_cast<bool>(editor.edit("history.undo"))&&editor.context().documentContent()==beforePlacement,"Model attachment and node creation undo as one document operation");
            require(!editor.edit("node.place",{{"resource",id},{"id","invalid-text-placement"}}),"Placement rejects registered text assets");
            require(editor.context().documentContent()==beforePlacement,"Rejected asset attachment leaves the document intact");
            require(static_cast<bool>(editor.edit("node.place",{{"resource",modelId},{"id","persistent-placement"}})),"Place persistent generated asset");
            require(static_cast<bool>(editor.edit("component.add",{{"type","azure.animator"}})),"Add authored animation component");
            require(static_cast<bool>(editor.edit("document.save")),"Save generated model document");
            const auto savedContent=editor.context().documentContent();
            require(static_cast<bool>(editor.edit("document.reload")),"Reload generated model document");
            if(editor.context().documentContent()!=savedContent)std::cerr<<Json::diff(savedContent,editor.context().documentContent()).dump(2)<<'\n';
            require(editor.context().documentContent()==savedContent,"Generated model document survives save and reload");
            auto invalidModel=modelParameters;invalidModel["parameters"]["source"]="{}";
            reject([&]{assets->validate(artifact(Json::array({operation("asset.generate",invalidModel)})).dump(),projectContext);});
            auto external=Json::parse(modelSource);external["buffers"][0]["uri"]="outside.bin";invalidModel["parameters"]["source"]=external.dump();
            reject([&]{assets->validate(artifact(Json::array({operation("asset.generate",invalidModel)})).dump(),projectContext);});
        }
        std::filesystem::remove_all(directory);std::cout<<"Two domain adapters, bounds, references, scope, preview and generation source passed\n";
    }catch(const std::exception& error){std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}
}
