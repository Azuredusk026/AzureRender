#include "rhi/PipelineInspection.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace azurerender::rhi {

PipelineInspection::PipelineInspection(VkDevice device, bool supported, bool requested,
                                     PFN_vkGetPipelineExecutablePropertiesKHR query) noexcept
    : device_(device), query_(query) {
    report_.supported = supported;
    report_.requested = requested;
    report_.entryPointAvailable = query != nullptr;
    report_.enabled = supported && requested && query && device;
}

void PipelineInspection::inspect(VkPipeline pipeline) noexcept {
    if (!report_.enabled) { ++report_.skipped; return; }
    VkResult result = VK_ERROR_UNKNOWN;
    if (pipeline) {
        VkPipelineInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
        info.pipeline = pipeline;
        PipelineInspectionRecord record;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            std::uint32_t count = 0;
            result = query_(device_, &info, &count, nullptr);
            if (result != VK_SUCCESS) break;
            if (!count || count > record.executables.size()) { result = VK_ERROR_UNKNOWN; break; }
            for (auto& executable : record.executables)
                executable = {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR};
            const auto capacity = count;
            result = query_(device_, &info, &count, record.executables.data());
            if (result == VK_INCOMPLETE) continue;
            if (result != VK_SUCCESS) break;
            if (!count || count > capacity) { result = VK_ERROR_UNKNOWN; break; }
            record.executableCount = count;
            for (std::uint32_t i = 0; i < count; ++i) {
                // No driver-owned pointer enters the durable report.
                record.executables[i].pNext = nullptr;
                record.executables[i].name[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
                record.executables[i].description[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
            }
            if (report_.recordCount < 64) {
                try {
                    if (report_.records.empty()) report_.records.reserve(64);
                    report_.records.push_back(record);
                    ++report_.recordCount;
                } catch (...) { result = VK_ERROR_OUT_OF_HOST_MEMORY; break; }
            }
            ++report_.inspected;
            return;
        }
    }
    ++report_.failed;
    report_.lastResult = result;
}

std::string PipelineInspection::jsonReport() const {
    nlohmann::json records = nlohmann::json::array();
    for (std::size_t i = 0; i < report_.recordCount; ++i) {
        nlohmann::json executables = nlohmann::json::array();
        const auto& record = report_.records[i];
        for (std::uint32_t j = 0; j < record.executableCount; ++j) {
            const auto& item = record.executables[j];
            executables.push_back({{"name", item.name}, {"description", item.description},
                                   {"stages", item.stages}, {"subgroupSize", item.subgroupSize}});
        }
        records.push_back({{"executables", executables}});
    }
    return nlohmann::json{{"supported", report_.supported}, {"requested", report_.requested},
        {"entryPointAvailable", report_.entryPointAvailable}, {"enabled", report_.enabled},
        {"inspected", report_.inspected}, {"failed", report_.failed}, {"skipped", report_.skipped},
        {"lastResult", report_.lastResult}, {"records", records}}.dump();
}

} // namespace azurerender::rhi
