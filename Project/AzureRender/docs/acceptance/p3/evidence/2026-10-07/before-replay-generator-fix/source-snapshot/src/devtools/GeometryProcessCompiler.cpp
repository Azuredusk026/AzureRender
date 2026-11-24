#include "GeometryProcessCompiler.hpp"
#include "platform/ProcessRunner.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <stdexcept>
namespace azurerender {
GeometryProcessCompiler::GeometryProcessCompiler(std::filesystem::path executable,std::filesystem::path scratchRoot):
    executable_(std::filesystem::absolute(executable)),scratch_(std::filesystem::absolute(scratchRoot)){}
std::string GeometryProcessCompiler::compile(const std::string& kind,const GenerationRequest& request){
    const auto typed=proceduralRequest(request);
    const auto& b=typed.budget;
    if(b.schemaVersion!=1||b.timeoutMs==0||b.timeoutMs>60000||b.sourceBytes==0||b.sourceBytes>2*1024*1024||b.depth==0||b.depth>64||b.triangles==0||b.triangles>1000000)
        throw std::invalid_argument("Invalid controlled geometry budget");
    auto size=typed.source.size();for(const auto& d:typed.dependencies)size+=d.bytes.size();
    if(size>b.sourceBytes)throw std::invalid_argument("Controlled geometry source budget exceeded");
    if(request.check)request.check();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(b.timeoutMs);
    static std::atomic<std::uint64_t> counter{0};
    std::filesystem::create_directories(scratch_);
    const auto directory=scratch_/(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++counter));
    if(!std::filesystem::create_directory(directory))throw std::runtime_error("Geometry candidate directory unavailable");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{directory};
    nlohmann::json envelope={{"schemaVersion",1},{"kind",kind},{"parameters",request.parameters},{"inputs",nlohmann::json::array()},{"dependencies",nlohmann::json::array()}};
    for(const auto& input:request.inputs)envelope["inputs"].push_back({{"reference",input.reference},{"bytes",input.bytes}});
    for(const auto& input:request.dependencies)envelope["dependencies"].push_back({{"reference",input.reference},{"bytes",input.bytes}});
    const auto input=directory/"request.json",output=directory/"candidate.asset";const auto bytes=envelope.dump();
    if(bytes.size()>16*1024*1024)throw std::invalid_argument("Geometry protocol envelope budget exceeded");
    {std::ofstream file(input,std::ios::binary);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));if(!file)throw std::runtime_error("Cannot write geometry request snapshot");}
    const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
    if(remaining<=0)throw std::runtime_error("Geometry generation deadline exceeded");
    ProcessRequest process;process.executable=executable_;process.workingDirectory=directory;process.arguments={input.u8string(),output.u8string()};process.timeoutMs=static_cast<std::uint32_t>(remaining);
    std::atomic<bool> cancelled{false};auto job=std::async(std::launch::async,[&]{return runProcess(process,cancelled);});
    try{
        while(job.wait_for(std::chrono::milliseconds(5))!=std::future_status::ready){if(request.check)request.check();if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Geometry generation deadline exceeded");}
        const auto result=job.get();if(!result.passed)throw std::runtime_error(result.diagnostic+": "+result.output);
    }catch(...){cancelled=true;if(job.valid())job.wait();throw;}
    if(request.check)request.check();
    const auto outputSize=std::filesystem::file_size(output);if(outputSize>128*1024*1024)throw std::runtime_error("Controlled geometry output budget exceeded");
    std::ifstream file(output,std::ios::binary);std::string generated(static_cast<std::size_t>(outputSize),'\0');
    for(std::size_t offset=0;offset<generated.size();){
        if(request.check)request.check();if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Geometry output read deadline exceeded");
        const auto count=std::min<std::size_t>(65536,generated.size()-offset);file.read(generated.data()+offset,static_cast<std::streamsize>(count));
        if(file.gcount()!=static_cast<std::streamsize>(count))throw std::runtime_error("Geometry output changed during read");offset+=count;
    }
    return generated;
}
}
