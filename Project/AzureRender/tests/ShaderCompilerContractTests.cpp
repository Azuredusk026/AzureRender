#include "devtools/ShaderHotReloader.hpp"
#include "devtools/ShaderCompileJob.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed,"Invalid compiler configuration accepted");}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream f(path,std::ios::binary);f<<text;if(!f)throw std::runtime_error("Cannot save compiler fixture");}
std::string read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Compiler output unavailable");return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
int snapshotTiming(const char* config){
    const auto options=loadShaderReloadOptions(config);ShaderSourceSnapshot snapshot;nlohmann::json times=nlohmann::json::array();
    const auto elapsed=[](auto start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
    for(unsigned i=0;i<3;++i){const auto start=std::chrono::steady_clock::now();snapshot=snapshotShaders(options);times.push_back(elapsed(start));}
    std::size_t bytes=0;for(const auto& entry:snapshot.files)bytes+=entry.second.size();
    std::uint64_t ranged=14695981039346656037ULL,pointer=ranged;
    auto start=std::chrono::steady_clock::now();for(const auto& entry:snapshot.files)for(const unsigned char c:entry.second){ranged^=c;ranged*=1099511628211ULL;}const auto rangedMs=elapsed(start);
    start=std::chrono::steady_clock::now();for(const auto& entry:snapshot.files){auto p=reinterpret_cast<const unsigned char*>(entry.second.data());const auto end=p+entry.second.size();for(;p!=end;++p){pointer^=*p;pointer*=1099511628211ULL;}}const auto pointerMs=elapsed(start);require(ranged==pointer,"Byte pointer hash differs from ranged hash");
    start=std::chrono::steady_clock::now();for(const auto& entry:snapshot.files)require(read(options.sourceRoot/entry.first)==entry.second,"Stream source read differs");const auto streamMs=elapsed(start);
    start=std::chrono::steady_clock::now();for(const auto& entry:snapshot.files){std::ifstream f(options.sourceRoot/entry.first,std::ios::binary|std::ios::ate);const auto size=f.tellg();require(size>=0,"Cannot size source read");std::string value(static_cast<std::size_t>(size),'\0');f.seekg(0);f.read(value.data(),static_cast<std::streamsize>(value.size()));require(bool(f)&&value==entry.second,"Block source read differs");}const auto blockMs=elapsed(start);
    std::cout<<nlohmann::json{{"programs",options.programs.size()},{"files",snapshot.files.size()},{"bytes",bytes},{"fingerprint",snapshot.fingerprint},{"snapshotMilliseconds",times},
        {"rangeHashMilliseconds",rangedMs},{"pointerHashMilliseconds",pointerMs},{"streamReadMilliseconds",streamMs},{"blockReadMilliseconds",blockMs},{"hashesExact",true},{"sourceBytesExact",true}}.dump(2)<<'\n';return 0;
}
void wait(ShaderHotReloader& service){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
    while(service.status().state==ShaderReloadState::Compiling){service.poll();require(std::chrono::steady_clock::now()<deadline,"Compiler fixture exceeded deadline");std::this_thread::sleep_for(std::chrono::milliseconds(5));}}
}
int main(int argc,char** argv)try{
    if(argc==3&&std::string(argv[1])=="--snapshot-timing")return snapshotTiming(argv[2]);
    require(argc==4,"Expected generated descriptor, DXC and new work directory");
    const auto generated=loadShaderReloadOptions(argv[1]);const auto complete=snapshotShaders(generated);
    require(!complete.files.empty()&&!complete.fingerprint.empty(),"Complete source snapshot is empty");
    const auto root=std::filesystem::absolute(std::filesystem::u8path(argv[3]));require(std::filesystem::create_directory(root),"Compiler work directory must be new");
    std::filesystem::create_directory(root/"source");std::filesystem::create_directory(root/"verified");
    write(root/"source/probe.comp","#version 450\nlayout(local_size_x=1) in;layout(binding=0) buffer Output {uint value;};void main(){value=7;}\n");
    const std::string hlsl="RWStructuredBuffer<uint> output:register(u0);[numthreads(1,1,1)]void main(){output[0]=VALUE;}\n";write(root/"source/probe.hlsl",hlsl);
    const std::vector<std::string> arguments{"-spirv","-T","cs_6_0","-E","main","-HV","2018","-fspv-target-env=vulkan1.1","-Od"};
    nlohmann::json descriptor={{"schemaVersion",1},{"sourceRoot","source"},{"binaryRoot","empty"},{"scratchRoot","candidates"},{"compiler",generated.compiler.u8string()},
        {"programs",{{{"source","probe.comp"},{"output","probe.comp.spv"}},{{"source","probe.hlsl"},{"output","probe.hlsl.spv"},{"defines",{"VALUE=7u"}},
        {"compiler",std::filesystem::absolute(std::filesystem::u8path(argv[2])).u8string()},{"arguments",arguments},{"outputOption","-Fo"}}}}};
    write(root/"reload.json",descriptor.dump(2));const auto options=loadShaderReloadOptions(root/"reload.json");
    require(options.programs[1].compiler==std::filesystem::absolute(std::filesystem::u8path(argv[2]))&&options.programs[1].arguments==arguments&&options.programs[1].outputOption=="-Fo","Per-program compiler descriptor differs");
    const auto snapshot=snapshotShaders(options);auto changed=options;changed.programs[1].arguments.push_back("-DUNUSED=1");require(snapshotShaders(changed).fingerprint!=snapshot.fingerprint,"Compiler arguments must participate in fingerprint");
    changed=options;changed.programs[1].compiler=root/"missing compiler.exe";require(snapshotShaders(changed).fingerprint!=snapshot.fingerprint,"Compiler identity must participate in fingerprint");
    changed=options;changed.programs[1].outputOption="-o";require(snapshotShaders(changed).fingerprint!=snapshot.fingerprint,"Output option must participate in fingerprint");
    changed=options;changed.programs[1].arguments.assign(65,"-Od");rejects([&]{snapshotShaders(changed);});
    changed=options;changed.programs[1].arguments={std::string(4097,'x')};rejects([&]{snapshotShaders(changed);});
    changed=options;changed.programs[1].outputOption="--unknown";rejects([&]{snapshotShaders(changed);});
    changed=options;changed.programs.clear();for(unsigned i=0;i<512;++i){auto program=options.programs[0];program.output="probe-"+std::to_string(i)+".spv";changed.programs.push_back(program);}require(!snapshotShaders(changed).fingerprint.empty(),"512 bounded programs must be supported");
    changed.programs.push_back(options.programs[1]);rejects([&]{snapshotShaders(changed);});
    ShaderHotReloader service(options);service.requestRebuild();wait(service);require(service.status().state==ShaderReloadState::Ready,"Mixed GLSL/HLSL compilation failed");
    auto candidate=service.candidate();require(candidate.has_value(),"Complete mixed candidate missing");
    for(const auto name:{"probe.comp.spv","probe.hlsl.spv"}){require(read(candidate->directory/name).size()>20,"Real SPIR-V output missing");std::filesystem::copy_file(candidate->directory/name,root/"verified"/name);}
    service.finishCandidate(true,{});const auto active=service.status().activeGeneration;
    write(root/"source/probe.hlsl","invalid shader syntax\n");service.requestRebuild();wait(service);
    require(service.status().state==ShaderReloadState::Error&&service.status().activeGeneration==active,"HLSL failure must preserve active generation");
    require(read(candidate->directory/"probe.hlsl.spv")==read(root/"verified/probe.hlsl.spv"),"Failed HLSL changed active bytes");
    write(root/"source/probe.hlsl",hlsl);service.requestRebuild();wait(service);require(service.status().state==ShaderReloadState::Ready,"Mixed compilation did not recover");service.finishCandidate(true,{});
    service.requestRebuild();service.cancel();wait(service);require(service.status().state==ShaderReloadState::Cancelled,"Mixed compilation cancellation failed");
    changed=options;changed.programs[1].compiler=root/"missing compiler.exe";ShaderHotReloader absent(changed);absent.requestRebuild();wait(absent);require(absent.status().state==ShaderReloadState::Error&&!absent.status().diagnostic.empty(),"Missing per-program compiler needs a diagnostic");
    auto projected=generated;projected.programs.clear();for(const auto& program:generated.programs)if(program.output=="projected_reflection_trace.comp.spv")projected.programs.push_back(program);
    require(projected.programs.size()==1&&!projected.programs[0].compiler.empty(),"Generated projected compiler descriptor missing");
    const auto expected=read(generated.binaryRoot/projected.programs[0].output);projected.binaryRoot=root/"empty";projected.scratchRoot=root/"projected";
    const std::atomic<bool> canceled{false};const auto actual=compileShaderCandidate(projected,snapshotShaders(projected),1,canceled);require(actual.passed,"Actual projected reload compilation failed");
    require(read(actual.candidate.directory/projected.programs[0].output)==expected,"Reload and normal-build projected SPIR-V differ");
    std::filesystem::copy_file(actual.candidate.directory/projected.programs[0].output,root/"verified/projected.spv");
    const nlohmann::json result={{"status","passed"},{"generatedPrograms",generated.programs.size()},{"programCapacity",512},{"mixedPrograms",2},{"projectedBytes",expected.size()},
        {"projectedBuildExact",true},{"hlslFailurePreservesActive",true},{"mixedRecovery",true},{"cancellation",true},{"missingCompilerRejected",true}};
    write(root/"result.json",result.dump(2)+"\n");std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
