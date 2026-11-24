#include "render/ProjectedReflectionPreparation.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}require(failed,"Invalid projected preparation accepted");}
}
int main()try{
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    {
        ProjectedReflectionPreparationGpu resource(backend,{35,19},2,code,2);
        const auto first=resource.output(0),second=resource.output(1);
        require(first.extent.width==17&&first.extent.height==9,"Preparation must floor half odd dimensions");
        require(resource.imageByteSize()==17*9*25*2&&backend.allocator().statistics().liveImages==8,"Preparation image budget differs");
        require(first.depth!=second.depth&&first.normalRoughness!=second.normalRoughness&&first.footprint!=second.footprint,"Preparation slots must be independent");
        require(first.tiles==second.tiles&&first.tileCount==6&&first.tileBytes==24,"Preparation must share a complete bounded tile list");
        ProjectedReflectionEligibilityTarget target{reinterpret_cast<VkImage>(900),reinterpret_cast<VkImageView>(901),{17,9}};
        const auto source=reinterpret_cast<VkImageView>(902);const std::array<float,2> decode{{10,.001F}};
        ProjectedReflectionPreparationParameters params;rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=target;bad.extent.width=18;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
        bad=target;bad.image=VK_NULL_HANDLE;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
        bad=target;bad.view=VK_NULL_HANDLE;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
        bad=target;bad.view=source;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
        for(const auto own:{first,second}){
            for(auto view:{own.depth,own.normalRoughness,own.mask,own.footprint}){
                rejects([&]{resource.record(commands,0,view,target,decode,params);});
                bad=target;bad.view=view;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
            }
            bad=target;bad.image=own.depthImage;rejects([&]{resource.record(commands,0,source,bad,decode,params);});
        }
        auto p=params;p.maxRoughness=0;rejects([&]{resource.record(commands,0,source,target,decode,p);});
        p=params;p.strength=std::numeric_limits<float>::quiet_NaN();rejects([&]{resource.record(commands,0,source,target,decode,p);});
        p=params;p.strength=1.1F;rejects([&]{resource.record(commands,0,source,target,decode,p);});
        rejects([&]{resource.record(commands,0,source,target,{0,0},params);});
        rejects([&]{resource.record(commands,0,source,target,{10,-1},params);});
        rejects([&]{resource.record(commands,0,VK_NULL_HANDLE,target,decode,params);});
        rejects([&]{resource.record(commands,2,source,target,decode,params);});
        require(commands.calls.empty()&&backend.descriptorImageWrites.size()==writes,"Preparation rejection must precede mutations");
        resource.record(commands,0,source,target,decode,params);require(!commands.calls.empty(),"Preparation did not record");
        // Recording may be discarded before submission. Re-recording needs no host layout reset.
        commands.calls.clear();resource.record(commands,0,source,target,decode,params);require(!commands.calls.empty(),"Preparation retry did not record");
        resource.record(commands,1,source,target,decode,params);
    }
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Preparation ownership leaked");
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{8192,8192},2,code);});
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{2048,2048},1,code);}); // 65536 tiles within image budget.
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{0,19},2,code);});
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{35,19},0,code);});
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{35,19},2,code,3);});
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{35,19},2,{});});
    backend.unavailableStorageFormats.insert(VK_FORMAT_R16G16_UNORM);
    rejects([&]{ProjectedReflectionPreparationGpu resource(backend,{35,19},2,code);});
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Rejected preparation allocated resources");
    std::cout<<"PASS: projected preparation ownership, tiles, bounds, aliases and retry contract\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
