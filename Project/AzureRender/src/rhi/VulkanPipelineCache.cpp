#include "rhi/VulkanPipelineCache.hpp"
#include <algorithm>
#include <stdexcept>

namespace azurerender::rhi {
namespace {
PipelineCacheIdentity identify(VkPhysicalDevice physical){
    if(!physical)throw std::invalid_argument("Pipeline cache requires a physical device");
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical,&properties);
    PipelineCacheIdentity result;result.vendor=properties.vendorID;result.device=properties.deviceID;result.driver=properties.driverVersion;
    std::copy(std::begin(properties.pipelineCacheUUID),std::end(properties.pipelineCacheUUID),result.uuid.begin());return result;
}
}
VulkanPipelineCache::VulkanPipelineCache(VkDevice device,VkPhysicalDevice physical,std::filesystem::path directory,PipelineCacheLimits limits)
    :device_(device),store_(std::move(directory),identify(physical),limits),limits_(limits){
    if(!device_)throw std::invalid_argument("Pipeline cache requires a device");
    const auto bytes=store_.load();statistics_.readState=store_.file().empty()?"memory-only":"empty";
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};info.initialDataSize=bytes.size();info.pInitialData=bytes.empty()?nullptr:bytes.data();
    const auto result=vkCreatePipelineCache(device_,&info,nullptr,&cache_);
    if(result==VK_SUCCESS){statistics_.loadedBytes=bytes.size();if(!bytes.empty())statistics_.readState="loaded";return;}
    cache_=VK_NULL_HANDLE;
    if(!bytes.empty()){
        statistics_.readState="driver-rejected";info.initialDataSize=0;info.pInitialData=nullptr;
        if(vkCreatePipelineCache(device_,&info,nullptr,&cache_)!=VK_SUCCESS)cache_=VK_NULL_HANDLE;
    }
    if(!cache_)statistics_.readState="unavailable";
}
VulkanPipelineCache::~VulkanPipelineCache(){if(cache_)vkDestroyPipelineCache(device_,cache_,nullptr);}
bool VulkanPipelineCache::flush()noexcept{try{
    if(!cache_){statistics_.writeState="unavailable";return false;}
    if(store_.file().empty()){statistics_.writeState="memory-only";return false;}
    for(unsigned attempt=0;attempt<2;++attempt){
        std::size_t size=0;
        if(vkGetPipelineCacheData(device_,cache_,&size,nullptr)!=VK_SUCCESS){statistics_.writeState="driver-error";return false;}
        if(size<32||size>limits_.maximumFileBytes){statistics_.writeState="size-limit";return false;}
        PipelineCacheBytes bytes(size);const auto result=vkGetPipelineCacheData(device_,cache_,&size,bytes.data());
        if(result==VK_INCOMPLETE)continue;
        if(result!=VK_SUCCESS){statistics_.writeState="driver-error";return false;}
        bytes.resize(size);if(!store_.save(bytes)){statistics_.writeState="not-stored";return false;}
        statistics_.writtenBytes=size;statistics_.writeState="stored";return true;
    }
    statistics_.writeState="data-changed";return false;
}catch(...){statistics_.writeState="io-error";return false;}}
} // namespace azurerender::rhi
