#include "ShaderCompileJob.hpp"
#include "platform/ProcessRunner.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace azurerender {
namespace {
bool validRelativeShaderPath(const std::filesystem::path& path){
    if(path.empty() || path.is_absolute())return false;
    for(const auto& part:path)if(part=="..")return false;
    return true;
}
std::string read(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Shader input unavailable: "+path.u8string());
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
}
ShaderSourceSnapshot snapshotShaders(const ShaderReloadOptions& options){
    if(options.programs.empty() || options.programs.size()>128 || options.sourceBytes==0 || options.sourceBytes>16*1024*1024
        || options.timeoutMs==0 || options.timeoutMs>60000 || options.pollIntervalMs==0)
        throw std::invalid_argument("Invalid shader reload configuration");
    ShaderSourceSnapshot result;std::size_t bytes=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(options.sourceRoot)){
        if(entry.is_symlink())throw std::invalid_argument("Shader snapshots require regular owned files");
        if(!entry.is_regular_file())continue;
        bytes+=static_cast<std::size_t>(entry.file_size());
        if(bytes>options.sourceBytes || result.files.size()>=4096)throw std::invalid_argument("Shader source exceeds budget");
        result.files.emplace(entry.path().lexically_relative(options.sourceRoot),read(entry.path()));
    }
    std::uint64_t hash=14695981039346656037ULL;
    auto append=[&](const std::string& text){for(const unsigned char c:text){hash^=c;hash*=1099511628211ULL;}hash^=0;hash*=1099511628211ULL;};
    for(const auto& [path,text]:result.files){append(path.generic_u8string());append(text);}
    for(const auto& program:options.programs){
        if(!validRelativeShaderPath(program.source) || !validRelativeShaderPath(program.output) || !result.files.count(program.source))
            throw std::invalid_argument("Shader program must reference a snapshot source and relative output");
        append(program.source.generic_u8string());append(program.output.generic_u8string());
        for(const auto& define:program.defines)append(define);
    }
    std::ostringstream fingerprint;fingerprint<<std::hex<<hash;result.fingerprint=fingerprint.str();return result;
}
ShaderCompileResult compileShaderCandidate(const ShaderReloadOptions& options,const ShaderSourceSnapshot& snapshot,
    std::uint64_t generation,const std::atomic<bool>& cancelled){
    ShaderCompileResult result;
    result.candidate={generation,options.scratchRoot/ std::to_string(generation)/"binary",snapshot.fingerprint};
    const auto sources=options.scratchRoot/std::to_string(generation)/"source";
    try{
        if(std::filesystem::exists(sources.parent_path()))throw std::runtime_error("Shader candidate generation already exists");
        std::filesystem::create_directories(result.candidate.directory);
        std::filesystem::create_directories(sources);
        for(const auto& [path,text]:snapshot.files){
            std::filesystem::create_directories((sources/path).parent_path());
            std::ofstream output(sources/path,std::ios::binary);output.write(text.data(),static_cast<std::streamsize>(text.size()));
            if(!output)throw std::runtime_error("Cannot persist shader snapshot");
        }
        if(std::filesystem::is_directory(options.binaryRoot))
            for(const auto& entry:std::filesystem::recursive_directory_iterator(options.binaryRoot))
                if(entry.is_regular_file() && entry.path().extension()==".spv"){
                    const auto target=result.candidate.directory/entry.path().lexically_relative(options.binaryRoot);
                    std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(entry.path(),target);
                }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(options.timeoutMs);
        for(const auto& program:options.programs){
            if(cancelled){result.cancelled=true;throw std::runtime_error("Shader compilation cancelled");}
            const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if(remaining<=0)throw std::runtime_error("Shader compilation deadline exceeded");
            const auto target=result.candidate.directory/program.output;
            std::filesystem::create_directories(target.parent_path());
            ProcessRequest request;request.executable=options.compiler;request.workingDirectory=sources;
            request.timeoutMs=static_cast<std::uint32_t>(remaining);
            for(const auto& define:program.defines)request.arguments.push_back("-D"+define);
            request.arguments.insert(request.arguments.end(),{"-I",sources.u8string(),(sources/program.source).u8string(),"-o",target.u8string()});
            const auto process=runProcess(request,cancelled);
            if(!process.passed){result.cancelled=process.cancelled;throw std::runtime_error(process.diagnostic+"\n"+process.output);}
            const auto code=read(target);std::uint32_t magic=0;
            if(code.size()>=4)std::memcpy(&magic,code.data(),4);
            if(code.size()<20 || code.size()%4 || magic!=0x07230203)throw std::runtime_error("Compiler candidate is not SPIR-V");
        }
        result.passed=true;
    }catch(const std::exception& error){result.diagnostic=std::string(error.what()).substr(0,65536);}
    return result;
}
}
