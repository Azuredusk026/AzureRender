#include "editor/ai/ProposalController.hpp"
#include "editor/EditorContext.hpp"
#include "editor/commands/EditTransaction.hpp"
#include "runtime/Level.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include <cmath>
#include <fstream>
#include <set>
namespace azurerender {
namespace {
using Json=nlohmann::json;
Json envelope(const std::string& source){
    if(source.size()>2*1024*1024)throw std::invalid_argument("Proposal source exceeds budget");
    const auto parsed=Json::parse(source);
    if(!parsed.is_object()||parsed.size()!=2||!parsed.at("schemaVersion").is_number_integer()||parsed.at("schemaVersion")!=1
        ||!parsed.at("operations").is_array()||parsed.at("operations").empty()||parsed.at("operations").size()>128)
        throw std::invalid_argument("Unsupported proposal schema or operation budget");
    return parsed;
}
std::vector<EditRequest> operations(const Json& artifact,const ProposalValidationContext& context,const std::set<std::string>& allowed){
    std::vector<EditRequest> requests;
    for(const auto& row:artifact.at("operations")){
        if(!row.is_object()||row.size()!=2||!row.at("command").is_string()||!row.at("parameters").is_object())throw std::invalid_argument("Invalid proposal operation fields");
        const auto command=row.at("command").get<std::string>();
        const auto* entry=context.edits.registry().find(command);
        if(!allowed.count(command)||!entry)throw std::invalid_argument("Operation is outside the adapter contract");
        EditRegistry::validate(row.at("parameters"),entry->descriptor.parameters);
        requests.push_back({context.runId+"-"+std::to_string(requests.size()+1),command,row.at("parameters"),context.baseVersion,{}});
    }
    return requests;
}
Json proposalSchema(){
    const Json operation={{"type","object"},{"additionalProperties",false},{"required",{"command","parameters"}},
        {"properties",{{"command",{{"type","string"}}},{"parameters",{{"type","object"}}}}}};
    const Json sequence={{"type","array"},{"minItems",1},{"maxItems",128},{"items",operation}};
    return {{"type","object"},{"additionalProperties",false},{"required",{"schemaVersion","operations"}},
        {"properties",{{"schemaVersion",{{"type","integer"},{"const",1}}},{"operations",sequence}}}};
}
class SceneAdapter final:public IProposalAdapter{
public:
    std::string id() const override{return "scene";}
    Json schema() const override{return proposalSchema();}
    Json snapshot(const ProposalValidationContext& context) const override{
        auto descriptors=Json::array();for(const auto& row:context.edits.describe())if(allowed().count(row.at("id").get<std::string>()))descriptors.push_back(row);
        auto types=Json::array();for(const auto& item:runtimeComponentRegistry().metadata().types())types.push_back(runtimeComponentRegistry().describe(item.first));
        return {{"document",context.document.documentContent()},{"operations",descriptors},{"componentTypes",types}};
    }
    ProposalCandidate validate(const std::string& source,const ProposalValidationContext& context) const override{
        ProposalCandidate candidate;candidate.operations=operations(envelope(source),context,allowed());
        if(context.targetId!="document"){
            if(candidate.operations.front().commandId!="node.select"||candidate.operations.front().parameters!=Json{{"id",context.targetId}})
                throw std::invalid_argument("A node proposal must select its stable target first");
            for(const auto& operation:candidate.operations){
                if(operation.commandId=="node.select"&&operation.parameters!=Json{{"id",context.targetId}})throw std::invalid_argument("Selection is outside the proposal target");
                if(operation.commandId!="node.select"&&operation.commandId!="node.rename"&&operation.commandId!="node.visible"
                    &&operation.commandId!="node.transform"&&operation.commandId!="component.add"&&operation.commandId!="component.field")
                    throw std::invalid_argument("A node proposal must remain inside its target scope");
            }
        }
        EditTransaction preview(context.document);EditService edits(preview.candidate(),context.edits.registry());
        auto trial=candidate.operations;for(auto& request:trial)request.baseVersion=edits.version();
        const auto result=edits.executeBatch(trial);
        if(!result)throw std::invalid_argument("Scene candidate violates production semantics: "+result.diagnostics.dump());
        candidate.diff=result.diff;return candidate;
    }
private:
    static const std::set<std::string>& allowed(){
        static const std::set<std::string> ids={"node.create","node.select","node.rename","node.visible","node.transform", "node.place", "prefab.place", "component.add", "component.field", "node.delete", "node.duplicate"};return ids;
    }
};
class AssetParameterAdapter final:public IProposalAdapter{
public:
    std::string id() const override{return "asset-parameters";}
    Json schema() const override{return proposalSchema();}
    Json snapshot(const ProposalValidationContext& context) const override{
        if(!context.document.isProject())throw std::invalid_argument("Asset parameters require a project session");
        auto references=Json::array();for(const auto& [id,record]:context.document.assets().records())references.push_back({{"id",id},{"path",record.virtualPath},{"hash",record.contentHash}});
        return {{"generators",context.generators.describe()},{"assets",references},{"operation",context.edits.registry().find("asset.generate")->descriptor.parameters},
            {"outputFormats",{".json",".azurelevel",".azureprefab"}},{"effects","one generated asset with versioned source metadata"}};
    }
    ProposalCandidate validate(const std::string& source,const ProposalValidationContext& context) const override{
        if(!context.document.isProject())throw std::invalid_argument("Asset parameters require a project session");
        ProposalCandidate candidate;candidate.transactional=false;candidate.operations=operations(envelope(source),context,{"asset.generate"});
        if(candidate.operations.size()!=1)throw std::invalid_argument("Asset parameters use one external-side-effect operation");
        const auto& parameters=candidate.operations.front().parameters;
        const auto output=parameters.at("output").get<std::string>();
        if(output.rfind("engine:/",0)==0||output.find(":/")==std::string::npos)throw std::invalid_argument("Generated asset requires a project virtual output");
        const auto path=context.document.project().resolve(output);const auto extension=path.extension();
        if(extension!=".json"&&extension!=".azurelevel"&&extension!=".azureprefab")throw std::invalid_argument("Adapter requires a registered text asset format");
        GenerationRequest request;request.outputReference=output;request.licenseSource=parameters.at("license").get<std::string>();request.parameters=parameters.at("parameters");
        const auto inputs=parameters.value("inputs",std::vector<std::string>{}),dependencies=parameters.value("dependencies",std::vector<std::string>{});
        std::size_t total=0;
        const auto collect=[&](const auto& references,auto& values){for(const auto& reference:references){
            const auto sourcePath=context.document.assets().resolveReference(reference);
            if(sourcePath==path)throw std::invalid_argument("Asset output cannot depend on itself");
            const auto count=std::filesystem::file_size(sourcePath);
            if(count>2*1024*1024-total)throw std::invalid_argument("Proposal input assets exceed source budget");total+=static_cast<std::size_t>(count);
            std::ifstream file(sourcePath,std::ios::binary);std::string bytes(static_cast<std::size_t>(count),'\0');file.read(bytes.data(),static_cast<std::streamsize>(count));
            if(file.gcount()!=static_cast<std::streamsize>(count)||file.peek()!=std::char_traits<char>::eof())throw std::invalid_argument("Proposal source changed during read");
            values.push_back({reference,std::move(bytes)});
        }};
        collect(inputs,request.inputs);collect(dependencies,request.dependencies);
        const auto result=context.generators.generate(parameters.at("generator").get<std::string>(),request);
        if(result.bytes.size()>2*1024*1024)throw std::invalid_argument("Generated proposal exceeds source budget");
        const auto data=Json::parse(result.bytes);if(!data.is_object())throw std::invalid_argument("Generated text asset requires an object");
        if(extension==".azurelevel"||extension==".azureprefab")static_cast<void>(Level::parse(data,context.document.assets()));
        else if(data.contains("states"))static_cast<void>(AnimationStateMachine::parse(data));
        std::string previous;
        if(std::filesystem::is_regular_file(path)){
            if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("Existing asset exceeds proposal source budget");
            std::ifstream file(path,std::ios::binary);previous.assign(std::istreambuf_iterator<char>(file),{});
        }
        candidate.diff.push_back({{"op","generate"},{"output",output},{"previousHash",generationHash(previous)},{"outputHash",generationHash(result.bytes)},
            {"inputs",result.manifest.inputHashes},{"dependencies",result.manifest.dependencyHashes},{"generator",result.manifest.generatorId},{"generatorVersion",result.manifest.generatorVersion}});
        return candidate;
    }
};
}
std::vector<std::shared_ptr<IProposalAdapter>> builtinProposalAdapters(){return {std::make_shared<SceneAdapter>(),std::make_shared<AssetParameterAdapter>()};}
}
