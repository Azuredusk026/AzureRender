#include "render/ResourceAccessProfile.hpp"
#include "render/ResourceIndexTable.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F function,const char* message){bool failed=false;try{function();}catch(const std::exception&){failed=true;}require(failed,message);}
struct OwnedBuffer {
    rhi::IGpuAllocator& allocator;
    rhi::GpuBuffer buffer;
    explicit OwnedBuffer(rhi::IGpuAllocator& value):allocator(value),buffer(value.createBuffer(32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true)){}
    ~OwnedBuffer(){allocator.destroyBuffer(buffer);}
};
}
int main(){
    try {
        DeviceCapabilities device;
        device.descriptorIndexingSupported=true;device.descriptorIndexingEnabled=true;
        auto& limits=device.limits;
        limits.maxPerStageDescriptorSamplers=64;limits.maxDescriptorSetSamplers=64;
        limits.maxPerStageDescriptorSampledImages=64;limits.maxDescriptorSetSampledImages=64;
        limits.maxPerStageResources=128;
        ResourceAccessRequirements request{35,11,4};
        require(ResourceAccessProfile::select(device,request).mode==ResourceAccessMode::Indexed,"Valid indexed layout rejected");
        for(unsigned field=0;field<5;++field){
            auto bounded=device;
            std::uint32_t* capacities[]={&bounded.limits.maxPerStageDescriptorSamplers,&bounded.limits.maxDescriptorSetSamplers,
                &bounded.limits.maxPerStageDescriptorSampledImages,&bounded.limits.maxDescriptorSetSampledImages,&bounded.limits.maxPerStageResources};
            *capacities[field]=field==4?38U:34U;
            const auto selection=ResourceAccessProfile::select(bounded,request);
            require(selection.mode==ResourceAccessMode::Fixed && !selection.diagnostic.empty(),"Each independent limit must select fixed access");
        }
        auto unsupported=device;unsupported.descriptorIndexingEnabled=false;
        require(ResourceAccessProfile::select(unsupported,request).mode==ResourceAccessMode::Fixed,"Supported feature must also be enabled");
        unsupported=device;unsupported.descriptorIndexingSupported=false;
        require(ResourceAccessProfile::select(unsupported,request).mode==ResourceAccessMode::Fixed,"Missing descriptor feature must use fixed access");
        unsupported=device;unsupported.limits={};
        require(ResourceAccessProfile::select(unsupported,request).mode==ResourceAccessMode::Fixed,"Unknown limits must use portable fixed access");
        unsupported=device;unsupported.limits.maxPerStageDescriptorSamplers=10;
        require(ResourceAccessProfile::select(unsupported,request).mode==ResourceAccessMode::Unsupported,"Impossible fixed layout must be diagnosed");
        auto address=request;address.requireDeviceAddress=true;
        device.deviceAddressSupported=true;
        require(ResourceAccessProfile::select(device,address).mode==ResourceAccessMode::Unsupported,"Address support alone is insufficient");
        device.deviceAddressEnabled=true;
        require(ResourceAccessProfile::select(device,address).mode==ResourceAccessMode::Unsupported,"Allocator address flag is required");
        device.allocatorDeviceAddressEnabled=true;
        require(ResourceAccessProfile::select(device,address).deviceAddress,"Enabled address contract must be observable");
        auto updates=request;updates.requireUpdateAfterBind=true;
        require(ResourceAccessProfile::select(device,updates).mode==ResourceAccessMode::Unsupported,"In-flight descriptor mutation requires an enabled contract");
        auto updating=device;updating.updateAfterBindSupported=true;updating.updateAfterBindEnabled=true;
        auto& updateLimits=updating.indexingLimits;
        updateLimits.maxPerStageDescriptorUpdateAfterBindSamplers=64;
        updateLimits.maxDescriptorSetUpdateAfterBindSamplers=64;
        updateLimits.maxPerStageDescriptorUpdateAfterBindSampledImages=64;
        updateLimits.maxDescriptorSetUpdateAfterBindSampledImages=64;
        updateLimits.maxPerStageUpdateAfterBindResources=64;
        updateLimits.maxUpdateAfterBindDescriptorsInAllPools=64;
        updating.limits.maxPerStageDescriptorSamplers=16;
        require(ResourceAccessProfile::select(updating,updates).mode==ResourceAccessMode::Indexed,
            "Update-after-bind layouts must use their separately queried limits");
        updateLimits.maxUpdateAfterBindDescriptorsInAllPools=38;
        require(ResourceAccessProfile::select(updating,updates).mode==ResourceAccessMode::Unsupported,
            "Update-after-bind pool-wide capacity must be checked");
        auto huge=request;huge.sampledImageSlots=std::numeric_limits<std::uint64_t>::max();
        require(ResourceAccessProfile::select(device,huge).mode==ResourceAccessMode::Fixed,"Resource sums must not overflow");
        auto invalid=request;invalid.fixedSampledImageSlots=0;
        rejects([&]{ResourceAccessProfile::select(device,invalid);},"Empty fixed layout must be rejected");

        rhi::NullRhi backend;
        ResourceIndexTable<OwnedBuffer> table(2);
        auto first=table.insert(std::make_shared<OwnedBuffer>(backend.allocator()));
        auto snapshot=table.acquire(first,7);
        const auto second=table.replace(first,std::make_shared<OwnedBuffer>(backend.allocator()));
        require(second.slot!=first.slot,"In-flight replacement must use a new slot");
        rejects([&]{table.acquire(first,8);},"Retired handle must be rejected immediately");
        require(backend.allocator().statistics().liveBuffers==2,"In-flight resource owner was released prematurely");
        snapshot.resource.reset();table.collect(6);
        require(backend.allocator().statistics().liveBuffers==2,"Fence retirement occurred too early");
        table.collect(7);
        require(backend.allocator().statistics().liveBuffers==1,"Completed resource was not released");
        auto secondSnapshot=table.acquire(second,8);secondSnapshot.resource.reset();
        const auto third=table.replace(second,std::make_shared<OwnedBuffer>(backend.allocator()));
        require(third.slot==first.slot && third.generation!=first.generation,"Reused slots require a new generation");
        rejects([&]{table.acquire(first,9);},"ABA handle must remain stale after reuse");
        table.collect(8);table.erase(third);table.collect(8);
        require(backend.allocator().statistics().liveBuffers==0,"Resource index table leaked allocation owners");
        rejects([&]{table.collect(7);},"Completed serial must be monotonic");
        ResourceIndexTable<int> one(1),other(1);
        const auto handle=one.insert(std::make_shared<const int>(1));
        const auto foreign=other.insert(std::make_shared<const int>(2));
        rejects([&]{one.acquire(foreign,1);},"Handles from another table must be rejected");
        auto retained=one.acquire(handle,2);retained.resource.reset();
        rejects([&]{one.replace(handle,std::make_shared<const int>(3));},"Full in-flight table must reject replacement");
        require(*one.acquire(handle,2).resource==1,"Failed replacement must retain active resource");
        one.collect(2);
        const auto replaced=one.replace(handle,std::make_shared<const int>(4));
        require(replaced.slot==handle.slot && replaced.generation!=handle.generation,"Completed slot can be replaced safely");
        rejects([&]{one.acquire(replaced,1);},"Submission serial must not precede completion");
        one.erase(replaced);one.collect(2);other.erase(foreign);other.collect(0);
        require(one.liveCount()==0 && one.retiredCount()==0,"Logical resource slots leaked");
        std::cout<<"Device limits, enabled capabilities, immutable index generations and fence retirement passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
