#include "assets/GeneratorRegistry.hpp"
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace azurerender {
namespace {
void reference(const std::string& name) {
    if(name.empty()||name.size()>1024||name.find('\\')!=std::string::npos||name.find("..")!=std::string::npos||name.front()=='/'||name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid generation reference");
    const auto colon=name.find(':');if(colon!=std::string::npos&&(name.substr(colon,2)!=":/"||name.find(':',colon+1)!=std::string::npos))throw std::invalid_argument("Generation references require virtual paths");
}
void hashMap(const std::map<std::string,std::string>& map) {
    for(const auto& entry:map) { reference(entry.first);if(!std::regex_match(entry.second,std::regex("fnv1a64:[0-9a-f]{16}")))throw std::invalid_argument("Invalid generation hash"); }
}
}
std::string generationHash(const std::string& bytes) {
    std::uint64_t hash=14695981039346656037ULL;for(const unsigned char value:bytes){hash^=value;hash*=1099511628211ULL;}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
nlohmann::json GenerationManifest::toJson() const {
    return {{"schemaVersion",schemaVersion},{"generatorId",generatorId},{"generatorVersion",generatorVersion},{"parametersHash",parametersHash},
        {"inputHashes",inputHashes},{"dependencyHashes",dependencyHashes},{"outputHashes",outputHashes},{"licenseSource",licenseSource}};
}
GenerationManifest GenerationManifest::fromJson(const nlohmann::json& value) {
    if(value.dump().size()>1024*1024||!value.at("schemaVersion").is_number_integer()||value.at("schemaVersion")!=1||!value.at("generatorVersion").is_number_integer())throw std::invalid_argument("Unsupported generation manifest");
    const auto version=value.at("generatorVersion").get<std::int64_t>();if(version<1||version>UINT32_MAX)throw std::invalid_argument("Invalid generator version");
    GenerationManifest result;result.generatorId=value.at("generatorId").get<std::string>();result.generatorVersion=static_cast<unsigned>(version);
    result.parametersHash=value.at("parametersHash").get<std::string>();result.licenseSource=value.at("licenseSource").get<std::string>();
    result.inputHashes=value.at("inputHashes").get<std::map<std::string,std::string>>();result.dependencyHashes=value.at("dependencyHashes").get<std::map<std::string,std::string>>();result.outputHashes=value.at("outputHashes").get<std::map<std::string,std::string>>();
    if(!std::regex_match(result.generatorId,std::regex("[A-Za-z][A-Za-z0-9_.-]{0,127}"))||result.licenseSource.empty()||result.licenseSource.size()>4096||result.outputHashes.empty()
        ||result.inputHashes.size()+result.dependencyHashes.size()+result.outputHashes.size()>1024)throw std::invalid_argument("Invalid generation identity or budget");
    hashMap({{"parameters",result.parametersHash}});hashMap(result.inputHashes);hashMap(result.dependencyHashes);hashMap(result.outputHashes);return result;
}
std::string GenerationManifest::fingerprint() const { return generationHash(toJson().dump()); }
void GeneratorRegistry::add(std::string id,unsigned version,Factory factory) {
    if(!std::regex_match(id,std::regex("[A-Za-z][A-Za-z0-9_.-]{0,127}"))||!version||!factory)throw std::invalid_argument("Invalid generator registration");
    if(!entries_.emplace(std::move(id),Entry{version,std::move(factory)}).second)throw std::invalid_argument("Duplicate generator");
}
GenerationResult GeneratorRegistry::generate(const std::string& id,const GenerationRequest& request) const {
    const auto found=entries_.find(id);if(found==entries_.end())throw std::invalid_argument("Unknown generator: "+id);
    if(request.parameters.dump().size()>2*1024*1024||request.inputs.size()+request.dependencies.size()>1023)throw std::invalid_argument("Generation input budget exceeded");
    if(request.check)request.check();GenerationResult result;auto& manifest=result.manifest;
    manifest.generatorId=id;manifest.generatorVersion=found->second.version;manifest.parametersHash=generationHash(request.parameters.dump());manifest.licenseSource=request.licenseSource;
    std::size_t bytes=0;
    const auto collect=[&](const auto& inputs,auto& hashes) { for(const auto& input:inputs) {
        reference(input.reference);bytes+=input.bytes.size();
        if(bytes>256*1024*1024||!hashes.emplace(input.reference,generationHash(input.bytes)).second)throw std::invalid_argument("Duplicate input or byte budget exceeded");
        if(request.check)request.check();
    } };
    collect(request.inputs,manifest.inputHashes);collect(request.dependencies,manifest.dependencyHashes);
    manifest.outputHashes[request.outputReference]=generationHash("");
    static_cast<void>(GenerationManifest::fromJson(manifest.toJson()));
    result.bytes=found->second.factory(request);
    if(result.bytes.size()>128*1024*1024)throw std::invalid_argument("Generated output exceeds 128 MiB");
    if(request.check)request.check();manifest.outputHashes[request.outputReference]=generationHash(result.bytes);result.fingerprint=manifest.fingerprint();return result;
}
nlohmann::json GeneratorRegistry::describe() const {
    auto value=nlohmann::json::array();for(const auto& entry:entries_)value.push_back({{"id",entry.first},{"version",entry.second.version}});return value;
}
GeneratorRegistry GeneratorRegistry::builtins() {
    GeneratorRegistry registry;
    registry.add("azure.text",1,[](const auto& r){return r.parameters.at("source").template get<std::string>();});
    registry.add("azure.binary-copy",1,[](const auto& r){if(r.inputs.size()!=1)throw std::invalid_argument("Copy requires one input");return r.inputs.front().bytes;});
    return registry;
}
}
