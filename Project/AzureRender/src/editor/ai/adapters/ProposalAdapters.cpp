#include "editor/ai/ProposalController.hpp"
#include "editor/EditorContext.hpp"
#include "editor/commands/EditTransaction.hpp"
#include "runtime/Level.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include "assets/GltfLoader.hpp"
#include "editor/material/MaterialEditingService.hpp"
#include "editor/content/ContentOperationService.hpp"
#include "runtime/AssetTypeRegistry.hpp"
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
        if(context.targetId!="document"&&std::none_of(context.document.scene().nodes.begin(),context.document.scene().nodes.end(),[&](const auto& node){return node.id==context.targetId;}))throw std::invalid_argument("Unknown scene proposal target");
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
        if(context.targetId!="document")throw std::invalid_argument("Asset generation requires document scope");
        auto references=Json::array();for(const auto& [id,record]:context.document.assets().records())references.push_back({{"id",id},{"path",record.virtualPath},{"hash",record.contentHash}});
        return {{"generators",context.generators.describe()},{"assets",references},{"operation",context.edits.registry().find("asset.generate")->descriptor.parameters},
            {"outputFormats",{".json",".azurelevel",".azureprefab",".gltf",".glb"}},{"effects","one generated asset with versioned source metadata"}};
    }
    ProposalCandidate validate(const std::string& source,const ProposalValidationContext& context) const override{
        if(!context.document.isProject())throw std::invalid_argument("Asset parameters require a project session");
        ProposalCandidate candidate;candidate.transactional=false;candidate.operations=operations(envelope(source),context,{"asset.generate"});
        if(candidate.operations.size()!=1)throw std::invalid_argument("Asset parameters use one external-side-effect operation");
        const auto& parameters=candidate.operations.front().parameters;
        const auto output=parameters.at("output").get<std::string>();
        if(output.rfind("engine:/",0)==0||output.find(":/")==std::string::npos)throw std::invalid_argument("Generated asset requires a project virtual output");
        const auto path=context.document.project().resolve(output);const auto extension=path.extension();
        if(extension!=".json"&&extension!=".azurelevel"&&extension!=".azureprefab"&&extension!=".gltf"&&extension!=".glb")throw std::invalid_argument("Adapter requires a registered asset format");
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
        if(extension==".gltf"||extension==".glb")static_cast<void>(loadEmbeddedGltfAsset(result.bytes,extension==".glb"));
        else{
            const auto data=Json::parse(result.bytes);if(!data.is_object())throw std::invalid_argument("Generated text asset requires an object");
            if(extension==".azurelevel"||extension==".azureprefab")static_cast<void>(Level::parse(data,context.document.assets()));
            else if(data.contains("states"))static_cast<void>(AnimationStateMachine::parse(data));
        }
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
class PhysicsAdapter final:public IProposalAdapter {
public:
    std::string id() const override{return "physics";}
    Json schema() const override{return proposalSchema();}
    Json snapshot(const ProposalValidationContext& context) const override{
        if(std::none_of(context.document.scene().nodes.begin(),context.document.scene().nodes.end(),[&](const auto& node){return node.id==context.targetId;}))throw std::invalid_argument("Physics proposal requires a stable node target");
        return {{"node",context.targetId},{"components",context.document.runtimeComponents().at(context.targetId)},
            {"types",{runtimeComponentRegistry().describe("azure.rigid-body"),runtimeComponentRegistry().describe("azure.hinge")}}};
    }
    ProposalCandidate validate(const std::string& source,const ProposalValidationContext& context) const override{
        static_cast<void>(snapshot(context));ProposalCandidate candidate;
        candidate.operations=operations(envelope(source),context,{"node.select","component.add","component.field","component.fields"});
        if(candidate.operations.front().commandId!="node.select"||candidate.operations.front().parameters!=Json{{"id",context.targetId}})throw std::invalid_argument("Physics proposal must select its stable target first");
        for(const auto& request:candidate.operations){
            if(request.commandId=="node.select"){if(request.parameters!=Json{{"id",context.targetId}})throw std::invalid_argument("Physics selection exceeds target scope");}
            else {const auto type=request.parameters.at("type").get<std::string>();if(type!="azure.rigid-body"&&type!="azure.hinge")throw std::invalid_argument("Physics proposal requires a registered physics component");}
        }
        EditTransaction transaction(context.document);EditService edits(transaction.candidate(),context.edits.registry());
        auto requests=candidate.operations;for(auto& request:requests)request.baseVersion=edits.version();
        const auto result=edits.executeBatch(requests);if(!result)throw std::invalid_argument("Physics candidate violates production semantics: "+result.diagnostics.dump());candidate.diff=result.diff;return candidate;
    }
};
class ContentDomainAdapter final:public IProposalAdapter {
    bool material_;
    std::string asset(const ProposalValidationContext& context) const {
        if(!context.document.isProject())throw std::invalid_argument("Content proposals require a project");
        const auto& record=context.document.assets().records().at(context.targetId);
        if(!assetTypeRegistry().matches(record.path,{material_?"material":"image"}))throw std::invalid_argument("Content target has the wrong registered type");
        return record.id;
    }
public:
    explicit ContentDomainAdapter(bool material):material_(material){}
    std::string id() const override{return material_?"material":"texture";}
    Json schema() const override{return proposalSchema();}
    Json snapshot(const ProposalValidationContext& context) const override{
        const auto target=asset(context);const auto& record=context.document.assets().records().at(target);
        if(material_){const auto document=MaterialDocument::load(record.path);const auto& type=MaterialTypeRegistry::builtins().type(document.typeId);
            return {{"asset",target},{"document",document.encode()},{"parameters",type.parameters},{"textures",type.textures},{"operation",context.edits.registry().find("material.field")->descriptor.parameters}};}
        ContentOperationService service(context.document.project());service.synchronize(context.document.assets());
        return {{"asset",target},{"settings",service.textureSettings({target})},{"operation",context.edits.registry().find("texture.settings")->descriptor.parameters}};
    }
    ProposalCandidate validate(const std::string& source,const ProposalValidationContext& context) const override{
        const auto target=asset(context);ProposalCandidate candidate;candidate.transactional=false;
        candidate.operations=operations(envelope(source),context,{material_?"material.field":"texture.settings"});
        if(candidate.operations.size()!=1)throw std::invalid_argument("Content proposals use one recoverable file transaction");
        const auto& args=candidate.operations.front().parameters;
        if(material_){
            if(args.at("asset")!=target)throw std::invalid_argument("Material proposal exceeds asset scope");
            const auto& record=context.document.assets().records().at(target);const auto before=MaterialDocument::load(record.path).encode();
            const auto after=MaterialEditingService::previewField(context.document,target,args.at("field").get<std::string>(),args.at("value")).encode();
            candidate.diff.push_back({{"asset",target},{"before",before},{"patch",Json::diff(before,after)}});
        }else{
            if(args.size()!=2||!args.contains("assets")||args.at("assets")!=Json::array({target})||!args.contains("values"))throw std::invalid_argument("Texture proposal requires only its stable target and settings");
            if(context.document.dirty())throw std::invalid_argument("Save the scene before changing texture import settings");
            ContentOperationService service(context.document.project());service.synchronize(context.document.assets());
            auto request=args;request["operation"]="texture.settings";const auto plan=service.plan(request);service.discard(plan.at("id"));
            candidate.diff.push_back({{"asset",target},{"before",service.textureSettings({target})},{"values",args.at("values")},{"version",plan.at("version")}});
        }
        return candidate;
    }
};
}
std::vector<std::shared_ptr<IProposalAdapter>> builtinProposalAdapters(){return {std::make_shared<SceneAdapter>(),std::make_shared<AssetParameterAdapter>(),std::make_shared<ContentDomainAdapter>(true),std::make_shared<ContentDomainAdapter>(false),std::make_shared<PhysicsAdapter>()};}
}
