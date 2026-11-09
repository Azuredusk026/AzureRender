#include "render/ScreenObjectVisibility.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid visibility input was accepted");}
std::array<float,16> identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
}
int main()try{
    rhi::NullRhi backend;
    std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    ScreenObjectVisibilityShaders shaders{code,code};
    backend.fragmentStorageBufferCount=0;
    require(!ScreenObjectVisibilityGpu::supported(backend,{19,11},2),"Missing fragment storage capacity was accepted");
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders);});
    backend.fragmentStorageBufferCount=8;
    ScreenObjectVisibilityNumerics reference;
    reference.mode=ScreenObjectVisibilityArithmetic::ProfileV1;
    reference.reciprocalCorrections.assign(524288,0x55555555u);
    backend.fragmentStorageBufferBytes=1048576;
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,reference);});
    backend.fragmentStorageBufferBytes=128ULL*1024*1024;
    auto& allocator=static_cast<rhi::NullGpuAllocator&>(backend.allocator());
    for(int failAfter=0;failAfter<5;++failAfter){
        allocator.allocationFailureAfter=failAfter;
        rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,reference);});
        require(!allocator.statistics().liveImages&&!allocator.statistics().liveBuffers,"Construction failure leaked resources");
        for(const auto& pair:{std::pair<const char*,const char*>{"createImageView","destroyImageView"},
            {"createSampler","destroySampler"},{"createDescriptorSetLayout","destroyDescriptorSetLayout"},
            {"createDescriptorPool","destroyDescriptorPool"},{"createPipelineLayout","destroyPipelineLayout"},
            {"createRenderPass","destroyRenderPass"},{"createFramebuffer","destroyFramebuffer"},
            {"createShaderModule","destroyShaderModule"},{"createGraphicsPipeline","destroyPipeline"}}){
            unsigned creates=0,destroys=0;
            for(const auto& call:backend.calls){creates+=call.name==pair.first;destroys+=call.name==pair.second;}
            require(creates==destroys,"Construction failure leaked a backend handle");
        }
    }
    allocator.allocationFailureAfter=-1;
    auto badNumerics=reference;badNumerics.reciprocalCorrections.pop_back();
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,badNumerics);});
    badNumerics=reference;badNumerics.reciprocalCorrections[17]|=3u;
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,badNumerics);});
    badNumerics=reference;badNumerics.mode=static_cast<ScreenObjectVisibilityArithmetic>(99);
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,badNumerics);});
    badNumerics=reference;badNumerics.mode=ScreenObjectVisibilityArithmetic::Device;
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders,badNumerics);});
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Rejected numerics allocated resources");
    {
        ScreenObjectVisibilityGpu deviceOwner(backend,{19,11},2,shaders);
        ScreenObjectVisibilityGpu referenceOwner(backend,{19,11},2,shaders,reference);
        require(referenceOwner.byteBudget()==deviceOwner.byteBudget()+2097152-4,"Reference profile budget mismatch");
        // The owner copies immutable configuration data during construction.
        reference.reciprocalCorrections.clear();
        ScreenObjectVisibilityInputs input;input.extent={19,11};
        input.depth=reinterpret_cast<VkImageView>(900);input.normals=reinterpret_cast<VkImageView>(901);
        input.objectIds=reinterpret_cast<VkImageView>(902);input.atlas=reinterpret_cast<VkImageView>(903);
        ScreenObjectVisibilityView view;view.clipToWorld=identity();
        ScreenObjectVisibilityRecord record;record.worldToUnitShadow=identity();
        rhi::NullCommandRecorder commands;
        for(unsigned slot:{0u,1u,0u})referenceOwner.record(commands,slot,input,view,{record});
        require(!commands.calls.empty(),"Reference mode produced no commands after source release");
    }
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Reference owner leaked resources");
    require(ScreenObjectVisibilityGpu::supported(backend,{19,11},2),"Small visibility output must be supported");
    for(const auto extent:{VkExtent2D{0,11},VkExtent2D{8193,1},VkExtent2D{8192,8192}})
        require(!ScreenObjectVisibilityGpu::supported(backend,extent,8),"Invalid or excessive visibility allocation was accepted");
    require(!ScreenObjectVisibilityGpu::supported(backend,{19,11},0),"Zero slots accepted");
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{8192,8192},8,shaders);});
    require(!backend.allocator().statistics().liveImages,"Rejected owner allocated images");
    backend.unavailableSampledFormats.insert(VK_FORMAT_R8G8_UNORM);
    require(!ScreenObjectVisibilityGpu::supported(backend,{19,11},2),"Unavailable output format was accepted");
    rejects([&]{ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders);});
    backend.unavailableSampledFormats.clear();
    {
        ScreenObjectVisibilityGpu owner(backend,{19,11},2,shaders);
        require(owner.output(0).view!=owner.output(1).view,"Slots must have distinct outputs");
        require(owner.output(0).layout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,"Visibility output must be sampled");
        require(owner.byteBudget()<19*11*4*2+4096,"Visibility output used a full-precision intermediate");
        ScreenObjectVisibilityInputs inputs;
        inputs.extent={19,11};
        inputs.depth=reinterpret_cast<VkImageView>(900);
        inputs.normals=reinterpret_cast<VkImageView>(901);
        inputs.objectIds=reinterpret_cast<VkImageView>(902);
        inputs.atlas=reinterpret_cast<VkImageView>(903);
        ScreenObjectVisibilityView view;view.clipToWorld=identity();
        ScreenObjectVisibilityRecord record;record.worldToUnitShadow=identity();
        const std::vector<ScreenObjectVisibilityRecord> records{record};
        rhi::NullCommandRecorder commands;
        const auto writes=backend.descriptorImageWrites.size();
        auto invalid=inputs;invalid.extent.width=18;rejects([&]{owner.record(commands,0,invalid,view,records);});
        invalid=inputs;invalid.depth=VK_NULL_HANDLE;rejects([&]{owner.record(commands,0,invalid,view,records);});
        invalid=inputs;invalid.objectIdFormat=VK_FORMAT_R8_UNORM;rejects([&]{owner.record(commands,0,invalid,view,records);});
        invalid=inputs;invalid.atlasLayout=VK_IMAGE_LAYOUT_GENERAL;rejects([&]{owner.record(commands,0,invalid,view,records);});
        invalid=inputs;invalid.normals=owner.output(1).view;rejects([&]{owner.record(commands,0,invalid,view,records);});
        backend.unavailableSampledFormats.insert(inputs.normalFormat);
        rejects([&]{owner.record(commands,0,inputs,view,records);});backend.unavailableSampledFormats.clear();
        auto badView=view;badView.clipToWorld.fill(0);rejects([&]{owner.record(commands,0,inputs,badView,records);});
        badView=view;badView.pixelOffset[0]=std::numeric_limits<float>::infinity();rejects([&]{owner.record(commands,0,inputs,badView,records);});
        badView=view;badView.phaseOrigin=static_cast<ScreenObjectVisibilityPhaseOrigin>(99);rejects([&]{owner.record(commands,0,inputs,badView,records);});
        auto badRecord=record;badRecord.atlasRect={.8F,0,.4F,1};rejects([&]{owner.record(commands,0,inputs,view,{badRecord});});
        badRecord=record;badRecord.lightDirection={0,0,0};rejects([&]{owner.record(commands,0,inputs,view,{badRecord});});
        badRecord=record;badRecord.filterRadiusUv[0]=-1;rejects([&]{owner.record(commands,0,inputs,view,{badRecord});});
        badRecord=record;badRecord.normalBias=std::numeric_limits<float>::quiet_NaN();rejects([&]{owner.record(commands,0,inputs,view,{badRecord});});
        rejects([&]{owner.record(commands,2,inputs,view,records);});
        rejects([&]{owner.record(commands,0,inputs,view,std::vector<ScreenObjectVisibilityRecord>(9,record));});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Rejected input changed descriptors or recording");
        owner.record(commands,0,inputs,view,records);
        require(!commands.calls.empty(),"Valid visibility input produced no pass");
        owner.record(commands,1,inputs,view,{});
        owner.record(commands,0,inputs,view,records);
    }
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Visibility owner leaked allocations");
    std::cout<<"PASS: visibility validation, bounded slots, aliases and release\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
