#pragma once
#include "rhi/PipelineCacheStore.hpp"
#include <vulkan/vulkan.h>
#include <string>

namespace azurerender::rhi {
struct VulkanPipelineCacheStatistics {
    std::uint64_t loadedBytes=0,writtenBytes=0;
    std::string readState="empty",writeState="not-requested";
};
// Borrows the device. Flush only after its pipeline-creation work has stopped,
// and destroy this owner before destroying the device.
class VulkanPipelineCache final {
public:
    VulkanPipelineCache(VkDevice,VkPhysicalDevice,std::filesystem::path directory,PipelineCacheLimits limits={});
    ~VulkanPipelineCache();
    VulkanPipelineCache(const VulkanPipelineCache&)=delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&)=delete;
    VkPipelineCache handle() const noexcept{return cache_;}
    std::filesystem::path file()const{return store_.file();}
    const VulkanPipelineCacheStatistics& statistics()const noexcept{return statistics_;}
    bool flush() noexcept;
private:
    VkDevice device_=VK_NULL_HANDLE;
    VkPipelineCache cache_=VK_NULL_HANDLE;
    PipelineCacheStore store_;
    PipelineCacheLimits limits_;
    VulkanPipelineCacheStatistics statistics_;
};
} // namespace azurerender::rhi
