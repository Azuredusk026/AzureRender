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
