#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace azurerender::rhi {

struct PipelineStatisticsOptions {
    bool requested=false;
    PFN_vkGetPipelineExecutableStatisticsKHR query=nullptr;
};
struct PipelineStatisticRecord {
    std::uint32_t executableIndex=0;
    VkPipelineExecutableStatisticKHR statistic{};
};
struct PipelineInspectionRecord {
    std::uint32_t executableCount = 0;
    std::array<VkPipelineExecutablePropertiesKHR, 32> executables{};
    std::vector<PipelineStatisticRecord> statistics;
    bool statisticsCaptured=false,statisticsTruncated=false;
};

struct PipelineInspectionReport {
    bool supported = false;
    bool requested = false;
    bool entryPointAvailable = false;
    bool enabled = false;
    std::uint64_t inspected = 0;
    std::uint64_t failed = 0;
    std::uint64_t skipped = 0;
    bool statisticsEnabled=false;
    bool statisticsRequested=false,statisticsEntryPointAvailable=false;
    std::uint64_t statisticsFailed=0;
    VkResult statisticsLastResult=VK_SUCCESS;
    VkResult lastResult = VK_SUCCESS;
    std::size_t recordCount = 0;
    std::vector<PipelineInspectionRecord> records;
};

// Borrows the device and pipelines; never destroys them. Calls are serialized
// by the device owner. Metadata remains valid after the device is destroyed.
class PipelineInspection {
public:
    PipelineInspection() = default;
    PipelineInspection(VkDevice device, bool supported, bool requested,
                       PFN_vkGetPipelineExecutablePropertiesKHR query,
                       PipelineStatisticsOptions statistics={}) noexcept;
    VkPipelineCreateFlags creationFlags() const noexcept {
        return report_.statisticsEnabled?VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR:0;
    }
    void inspect(VkPipeline pipeline,VkPipelineCreateFlags flags=0) noexcept;
    const PipelineInspectionReport& report() const noexcept { return report_; }
    std::string jsonReport() const;
private:
    VkDevice device_ = VK_NULL_HANDLE;
    PFN_vkGetPipelineExecutablePropertiesKHR query_ = nullptr;
    PFN_vkGetPipelineExecutableStatisticsKHR statistics_=nullptr;
    void inspectStatistics(VkPipeline,PipelineInspectionRecord&) noexcept;
    PipelineInspectionReport report_;
};

} // namespace azurerender::rhi
