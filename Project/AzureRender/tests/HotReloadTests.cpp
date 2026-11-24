#include "devtools/ShaderHotReloader.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
using namespace azurerender;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream(path)<<text;}
void wait(IShaderHotReloader& service){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
    while(service.status().state==ShaderReloadState::Compiling){
        service.poll();
        require(std::chrono::steady_clock::now()<deadline,"Compiler must finish within its deadline");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
int main(int argc,char** argv){try{
    require(argc==3,"Expected glslc and scratch directory");
    const auto root=std::filesystem::path(argv[2]);std::filesystem::create_directories(root/"source");
    const auto source=root/"source/probe.comp";
    write(source,"#version 450\n#extension GL_GOOGLE_include_directive : require\n#include \"shared.glsl\"\nlayout(local_size_x=1) in;\nvoid main(){uint value=VALUE;}\n");
    write(root/"source/shared.glsl","#define VALUE 7u\n");
    ShaderReloadOptions options;options.sourceRoot=root/"source";options.binaryRoot=root/"active";
    options.scratchRoot=root/"candidates";options.compiler=argv[1];options.pollIntervalMs=1;
    options.programs.push_back({"probe.comp","probe.comp.spv",{}});
    ShaderHotReloader service(options);service.requestRebuild();wait(service);
    require(service.status().state==ShaderReloadState::Ready,"A valid source must produce a candidate");
    const auto first=service.candidate();require(first.has_value(),"Ready exposes a complete candidate");
    require(std::filesystem::file_size(first->directory/"probe.comp.spv")>20,"Candidate holds a real compiled shader");
    service.finishCandidate(true,{});const auto accepted=service.status().activeGeneration;
    write(root/"source/shared.glsl","#define VALUE invalid syntax\n");
    service.requestRebuild();wait(service);
    require(service.status().state==ShaderReloadState::Error,"Invalid dependency must reject the candidate");
    require(service.status().activeGeneration==accepted,"Compilation failure preserves the active generation");
    require(std::filesystem::is_regular_file(first->directory/"probe.comp.spv"),"The valid shader remains available after failure");
    require(!service.status().diagnostic.empty(),"Compiler diagnostics are observable");
    write(root/"source/shared.glsl","#define VALUE 9u\n");
    service.requestRebuild();service.requestRebuild();wait(service);
    require(service.status().state==ShaderReloadState::Ready,"A subsequent valid dependency must recover");
    const auto recovered=service.candidate();require(recovered->fingerprint!=first->fingerprint,"Dependency bytes participate in the candidate fingerprint");
    service.finishCandidate(false,"Pipeline rejected");
    require(service.status().activeGeneration==accepted,"Pipeline creation failure preserves the active generation");
    service.requestRebuild();service.cancel();wait(service);
    require(service.status().state==ShaderReloadState::Cancelled,"Cancellation finishes the owned compile job");
    auto missing=options;missing.compiler=root/"missing compiler.exe";
    ShaderHotReloader absent(missing);absent.requestRebuild();wait(absent);
    require(absent.status().state==ShaderReloadState::Error,"Missing compiler yields a recoverable diagnostic");
#ifdef _WIN32
    service.requestRebuild();wait(service);
    const auto next=service.candidate();require(next.has_value(),"Recovery creates another complete candidate");
    const auto locked=CreateFileW((first->directory/"probe.comp.spv").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    require(locked!=INVALID_HANDLE_VALUE,"Fixture locks the accepted generation against deletion");
    try{service.finishCandidate(true,{});}catch(...){CloseHandle(locked);throw;}
    CloseHandle(locked);
    require(service.status().activeGeneration==next->generation,"Retirement failure cannot roll back a committed generation");
    require(!service.status().diagnostic.empty(),"Deferred retirement reports its cleanup diagnostic");
#endif
    std::cout<<"Shader dependencies, candidate preservation, recovery and cancellation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
