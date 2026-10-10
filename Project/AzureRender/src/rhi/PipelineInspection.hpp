#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace azurerender::rhi {

struct PipelineInspectionRecord {
    std::uint32_t executableCount = 0;
    std::array<VkPipelineExecutablePropertiesKHR, 32> executables{};
};

struct PipelineInspectionReport {
    bool supported = false;
    bool requested = false;
    bool entryPointAvailable = false;
    bool enabled = false;
    std::uint64_t inspected = 0;
    std::uint64_t failed = 0;
    std::uint64_t skipped = 0;
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
                       PFN_vkGetPipelineExecutablePropertiesKHR query) noexcept;
    void inspect(VkPipeline pipeline) noexcept;
    const PipelineInspectionReport& report() const noexcept { return report_; }
    std::string jsonReport() const;
private:
    VkDevice device_ = VK_NULL_HANDLE;
    PFN_vkGetPipelineExecutablePropertiesKHR query_ = nullptr;
    PipelineInspectionReport report_;
};

} // namespace azurerender::rhi
