#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <atomic>
#include <cstdlib>
#include <cstddef>
#include <limits>
#include <new>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
namespace allocations {
std::atomic<std::size_t> live{0},peak{0},total{0};
struct Header {void* allocation;std::size_t size;};
void* acquire(std::size_t size,std::size_t alignment=alignof(std::max_align_t)) {
    size=std::max<std::size_t>(size,1);total.fetch_add(size,std::memory_order_relaxed);
    alignment=std::max(alignment,alignof(std::max_align_t));
    if(size>std::numeric_limits<std::size_t>::max()-alignment-sizeof(Header))throw std::bad_alloc();
    auto* raw=std::malloc(size+alignment+sizeof(Header));
    if(!raw)throw std::bad_alloc();
    auto address=(reinterpret_cast<std::uintptr_t>(raw)+sizeof(Header)+alignment-1)&~(alignment-1);
    auto* header=reinterpret_cast<Header*>(address)-1;*header={raw,size};
    const auto current=live.fetch_add(size,std::memory_order_relaxed)+size;
    auto previous=peak.load(std::memory_order_relaxed);
    while(previous<current&&!peak.compare_exchange_weak(previous,current,std::memory_order_relaxed)){}
    return reinterpret_cast<void*>(address);
}
void release(void* pointer) noexcept {
    if(!pointer)return;
    const auto* header=static_cast<Header*>(pointer)-1;
    live.fetch_sub(header->size,std::memory_order_relaxed);std::free(header->allocation);
}
}
void* operator new(std::size_t size){return allocations::acquire(size);}
void* operator new[](std::size_t size){return allocations::acquire(size);}
void operator delete(void* pointer) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void* operator new[](std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void operator delete(void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void operator delete(void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void* operator new[](std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void operator delete(void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}


using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Measurement {std::size_t bytes;std::vector<rhi::RecordedCall> commands;};
Measurement measure(const std::filesystem::path& shaders,const std::filesystem::path& source,std::size_t vertexCount,bool bindless,unsigned aliases=0) {
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    context.allocator=&backend.allocator();context.rhi=&backend;context.shaderDirectory=shaders.string();
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
    context.renderExtent={128,96};context.bindlessTextures=bindless;context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;limits.maxPerStageResources=4096;
    const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").lexically_normal().generic_string();
    const auto other=(source/"assets_public/test_model.gltf").lexically_normal().generic_string();
    auto opaque=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    if(!vertexCount)vertexCount=opaque->vertices.size();
    require(vertexCount>=opaque->vertices.size(),"Opaque fixture must retain every referenced vertex");
    opaque->vertices.resize(vertexCount);
    for(auto& material:opaque->materials)material.alphaMode=AssetAlphaMode::Opaque;
    auto transparent=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    for(auto& material:transparent->materials)material.alphaMode=AssetAlphaMode::Blend;
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,opaque},{other,transparent}};
    context.preparedMeshes=&prepared;
    context.scene.resources.push_back({"opaque",path});context.scene.resources.push_back({"transparent",other});
    for(unsigned i=0;i<aliases;++i)context.scene.resources.push_back({"unused-alias-"+std::to_string(i),path});
    for(const auto* id:{"opaque","transparent"}) {scene::SceneNodeDesc node;node.id=id;node.resourceId=id;context.scene.nodes.push_back(node);}
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;context.renderSettings=&settings;
    renderer.onLoad(context);
    SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=128;frame.swapchainHeight=96;frame.cameraPosition[2]=5;
    renderer.updateFrame(frame);renderer.updateFrame(frame);
    const auto start=allocations::total.load();renderer.updateFrame(frame);
    const auto bytes=allocations::total.load()-start;
    rhi::NullCommandRecorder recorder;context.commands=&recorder;renderer.recordScene(context);
    require(std::any_of(recorder.calls.begin(),recorder.calls.end(),[](const auto& call){return call.name=="drawIndexed";}),"Mixed fixture must record geometry");
    const auto* state=renderer.sceneState();require(state&&state->pickables.size()>=2,"Mixed fixture must retain opaque and transparent instances");
    renderer.onUnload(context);
    require(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==0,"Mixed renderer must release all resources");
    return {bytes,std::move(recorder.calls)};
}
int main(int argc,char** argv) try {
    require(argc==3,"Expected shader and source directories");
    for(bool bindless:{false,true}) {
        const auto small=measure(argv[1],argv[2],0,bindless),large=measure(argv[1],argv[2],65536,bindless);
        std::cout<<"bindless="<<bindless<<" smallAllocationBytes="<<small.bytes<<" largeAllocationBytes="<<large.bytes<<std::endl;
        require(large.bytes<=small.bytes+64*1024,"Opaque vertices must not scale per-frame transparent scratch allocations");
        require(small.commands.size()==large.commands.size(),"Opaque-only vertex capacity must preserve recorded commands");
        for(std::size_t i=0;i<small.commands.size();++i)
            require(small.commands[i].name==large.commands[i].name&&small.commands[i].detail==large.commands[i].detail,"Opaque vertex capacity must preserve transparent ordering and draw inputs");
        const auto alias=measure(argv[1],argv[2],0,bindless,1024);
        std::cout<<"bindless="<<bindless<<" ordinaryAllocationBytes="<<small.bytes<<" alias1024AllocationBytes="<<alias.bytes<<std::endl;
        require(alias.bytes<=small.bytes+4096,"Unused loaded resource aliases must not scale per-frame layout allocations");
        require(small.commands.size()==alias.commands.size(),"Unused aliases must preserve the recorded draw count");
        for(std::size_t i=0;i<small.commands.size();++i)
            require(small.commands[i].name==alias.commands[i].name&&small.commands[i].detail==alias.commands[i].detail,"Unused aliases must preserve transparent ordering and every draw input");
    }
    std::cout<<"Opaque transparency scratch, descriptor paths, draw commands and resource release passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}
