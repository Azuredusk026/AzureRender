#include "runtime/AssetDatabase.hpp"
#include "runtime/Level.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include "assets/GltfLoader.hpp"
#include <fstream>
namespace azurerender {
namespace {
std::string read(const std::filesystem::path& path) { std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read generation source");return {std::istreambuf_iterator<char>(file),{}}; }
std::string readGenerationSource(const std::filesystem::path& path,std::uintmax_t size,const std::function<void()>& check) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read generation source");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    for(std::size_t offset=0;offset<bytes.size();) {
        if(check)check();const auto chunk=std::min<std::size_t>(64*1024,bytes.size()-offset);
        file.read(bytes.data()+offset,static_cast<std::streamsize>(chunk));
        if(file.gcount()!=static_cast<std::streamsize>(chunk))throw std::runtime_error("Generation source changed during read");
        offset+=chunk;
    }
    if(check)check();
    if(file.peek()!=std::char_traits<char>::eof()||std::filesystem::file_size(path)!=size)
        throw std::runtime_error("Generation source changed during read");
    if(file.bad())throw std::runtime_error("Cannot read generation source");return bytes;
}
void save(const std::filesystem::path& path,const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));file.close();if(!file)throw std::runtime_error("Cannot save generated asset");
}
}
std::string AssetDatabase::generateAsset(const GeneratorRegistry& generators,const std::string& generator,const std::string& output,
    const nlohmann::json& parameters,const std::vector<std::string>& inputs,const std::vector<std::string>& dependencies,
    const std::string& license,std::function<void(const std::string&)> validate,std::function<void()> check) {
    if(!validate||output.rfind("engine:/",0)==0)throw std::invalid_argument("Generation requires project output and content validator");
    const auto path=project_.resolve(output);const auto sidecar=std::filesystem::path(path.string()+".azmeta");
    if(path.extension()==".azmeta"||path.extension()==".tmp")throw std::invalid_argument("Generation output requires an asset extension");
    GenerationRequest request;request.parameters=parameters;request.outputReference=output;request.licenseSource=license;request.check=check;
    if(inputs.size()+dependencies.size()>1023||parameters.dump().size()>2*1024*1024)throw std::invalid_argument("Generation input budget exceeded");
    std::map<std::string,std::uintmax_t> sizes;std::uintmax_t total=0;
    for(const auto* references:{&inputs,&dependencies})for(const auto& reference:*references) {
        const auto id=reference.find(":/")==std::string::npos?reference:idForPath(reference);
        const auto& source=records_.at(id).path;if(source==path)throw std::invalid_argument("Generation output cannot depend on itself");
        const auto size=std::filesystem::file_size(source);
        if(size>256ULL*1024*1024-total)throw std::invalid_argument("Generation source byte budget exceeded");
        total+=size;sizes[reference]=size;
    }
    const auto collect=[&](const auto& references,auto& values) { for(const auto& reference:references) {
        const auto id=reference.find(":/")==std::string::npos?reference:idForPath(reference);
        if(records_.at(id).path==path)throw std::invalid_argument("Generation output cannot depend on itself");
        if(check)check();values.push_back({reference,readGenerationSource(records_.at(id).path,sizes.at(reference),check)});
    } };
    collect(inputs,request.inputs);collect(dependencies,request.dependencies);
    const auto result=generators.generate(generator,request);validate(result.bytes);if(check)check();
    const bool hadOutput=std::filesystem::exists(path),hadMeta=std::filesystem::exists(sidecar);
    const auto oldOutput=hadOutput?read(path):std::string(),oldMeta=hadMeta?read(sidecar):std::string();
    try {
        auto metadata=hadMeta?nlohmann::json::parse(oldMeta):nlohmann::json::object();
        // The regular asset importer owns UUID creation and source/cache semantics.
        save(path,result.bytes);
        if(!hadMeta) { AssetDatabase initialize=*this;initialize.refresh(true,check);metadata=nlohmann::json::parse(read(sidecar)); }
        metadata["generation"]=result.manifest.toJson();save(sidecar,metadata.dump(2));
        AssetDatabase candidate=*this;candidate.refresh(true,check);
        if(path.extension()==".azurelevel"||path.extension()==".azureprefab")static_cast<void>(Level::load(path,candidate));
        else if(path.extension()==".gltf"||path.extension()==".glb")static_cast<void>(loadGltfAsset(path.string()));
        else if(path.extension()==".json") { const auto document=nlohmann::json::parse(result.bytes);
            if(document.contains("states"))static_cast<void>(AnimationStateMachine::parse(document)); }
        if(check)check();const auto id=candidate.idForPath(output);records_.swap(candidate.records_);sources_.swap(candidate.sources_);statistics_=candidate.statistics_;return id;
    }catch(...) {
        if(hadOutput)save(path,oldOutput);else std::filesystem::remove(path);
        if(hadMeta)save(sidecar,oldMeta);else std::filesystem::remove(sidecar);
        throw;
    }
}
}
