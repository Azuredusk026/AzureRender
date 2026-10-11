#include "assets/AssetFormatRegistry.hpp"
#include "assets/AssetTransfer.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace azurerender;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool failed=false;try { action(); } catch (const std::exception&) { failed=true; }
    require(failed,message);
}
void write(const std::filesystem::path& path,const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<bytes;
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path(u8"azure 格式 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status=0;
    try {
        const auto& builtins=assetFormatRegistry();
        for(const std::string id:{"gltf","glb","obj","stl","ply","dae","fbx"})
            require(builtins.require(id).canRead&&builtins.require(id).settingsVersion==1,"Verified model readers must remain discoverable with supported settings");
        AssetFormatRegistry formats;
        auto descriptor=builtins.require("gltf");formats.add(descriptor);
        rejects([&]{formats.add(descriptor);},"Duplicate identity must be rejected");
        auto invalid=descriptor;invalid.id="invalid";invalid.extensions={"gltf"};
        rejects([&]{formats.add(invalid);},"Extensions require a leading dot");
        invalid=descriptor;invalid.id="version";invalid.settingsVersion=0;
        rejects([&]{formats.add(invalid);},"Settings must have a supported version");
        const auto source=root/std::filesystem::u8path(u8"输入 模型.GLTF");
        std::filesystem::create_directories(root);std::filesystem::copy_file(argv[1],source);
        require(formats.detect(source).id=="gltf","Uppercase extension and UTF-8 path must resolve by content");
        const auto disguised=root/"wrong.gltf";write(disguised,"not a glTF document");
        rejects([&]{formats.detect(disguised);},"Disguised extensions must fail content probing");
        auto duplicate=descriptor;duplicate.id="ambiguous";formats.add(duplicate);
        rejects([&]{formats.detect(source);},"Multiple matching probes must fail rather than select arbitrarily");
        AssetTransferRequest request;request.source=source;request.staging=root/"staging";request.destination=root/"published";
        request.formatId="gltf";
        request.settingsVersion=2;
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return false;});},"Unsupported settings versions must fail");
        require(!std::filesystem::exists(request.staging),"Invalid requests create no staging data");
        request.settingsVersion=1;
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return true;});},"Cancelled work must fail");
        require(!std::filesystem::exists(request.staging),"Cancelled preparation leaves no temporary data");
        auto prepared=prepareAssetTransfer(request,builtins,[] {return false;});
        require(prepared.summary.at("vertices").get<unsigned>()>0,"Transfer must use the actual model loader");
        require(!prepared.generation.inputHashes.empty(),"Transfer records source fingerprints");
        write(source,"changed source");
        rejects([&]{publishAssetTransfer(prepared);},"Stale source cannot publish prepared content");
        require(!std::filesystem::exists(request.destination)&&!std::filesystem::exists(request.staging),"Stale publication cleans its own staging data");
        std::filesystem::copy_file(argv[1],source,std::filesystem::copy_options::overwrite_existing);
        prepared=prepareAssetTransfer(request,builtins,[] {return false;});
        const auto result=publishAssetTransfer(prepared);
        require(std::filesystem::is_regular_file(result),"Validated content is published on the caller thread");
        require(!std::filesystem::exists(request.staging),"Publishing transfers ownership out of staging");
        request.destination=root/"tampered";
        prepared=prepareAssetTransfer(request,builtins,[] {return false;});
        write(prepared.staging/prepared.entry,"tampered output");
        rejects([&]{publishAssetTransfer(prepared);},"Tampered outputs must not publish");
        require(!std::filesystem::exists(request.staging),"Failed output validation cleans staging");
        request.maxSourceBytes=1;
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return false;});},"Size budgets must be enforced");
        require(!std::filesystem::exists(request.staging),"Oversized inputs leave no staging data");
        request.maxSourceBytes=512ULL*1024*1024;
        unsigned checks=0;
        rejects([&]{prepareAssetTransfer(request,builtins,[&] {return ++checks>6;});},"Cancellation during copying must clean partial output");
        require(!std::filesystem::exists(request.staging),"Mid-transfer cancellation cleans staging");
        write(request.staging/"keep.txt","user data");
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return false;});},"Occupied staging must be rejected");
        require(std::filesystem::exists(request.staging/"keep.txt"),"Rejecting an occupied staging path preserves existing files");
        std::filesystem::remove_all(request.staging);
        request.destination=root/"other";
        prepared=prepareAssetTransfer(request,builtins,[] {return false;});write(request.destination/"keep.txt","user data");
        rejects([&]{publishAssetTransfer(prepared);},"Occupied destination cannot be replaced implicitly");
        require(std::filesystem::exists(request.destination/"keep.txt"),"Conflict preserves destination content");
        request.staging=source.parent_path();
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return false;});},"Source and staging directories must be disjoint");
        std::ifstream stream(source);nlohmann::json document;stream>>document;
        document["buffers"][0]["uri"]="../outside.bin";write(source,document.dump());
        request.staging=root/"dependency-staging";request.destination=root/"dependency-target";
        rejects([&]{prepareAssetTransfer(request,builtins,[] {return false;});},"Dependency traversal must be rejected");
        require(!std::filesystem::exists(request.staging),"Dependency rejection cleans staging");
        std::ifstream original(argv[1]);original>>document;
        auto json=document.dump();while(json.size()%4)json+=' ';
        std::string binary;
        auto integer=[&](std::uint32_t value){for(unsigned i=0;i<4;++i)binary+=static_cast<char>((value>>(8*i))&255);};
        integer(0x46546c67);integer(2);integer(static_cast<std::uint32_t>(20+json.size()));
        integer(static_cast<std::uint32_t>(json.size()));integer(0x4e4f534a);binary+=json;
        request.source=root/std::filesystem::u8path(u8"二进制 模型.GLB");write(request.source,binary);request.formatId="glb";
        require(builtins.detect(request.source).id=="glb","GLB magic and case-insensitive extension must resolve");
        prepared=prepareAssetTransfer(request,builtins,[] {return false;});
        require(prepared.summary.at("vertices").get<unsigned>()>0,"Uppercase GLB must use the binary loader");
        discardAssetTransfer(prepared);
        std::cout<<"Format detection, versions, cancellation, stale publication and conflict recovery passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
