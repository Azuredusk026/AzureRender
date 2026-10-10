#include "rhi/PipelineInspection.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <nlohmann/json.hpp>

using namespace azurerender::rhi;
namespace {
unsigned calls = 0;
unsigned mode = 0;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
VkResult VKAPI_CALL query(VkDevice, const VkPipelineInfoKHR* info,
                         uint32_t* count, VkPipelineExecutablePropertiesKHR* properties) {
    ++calls;
    require(info->sType == VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR && info->pipeline,
            "Query requires a live borrowed pipeline");
    if (mode == 1) return VK_ERROR_UNKNOWN;
    if (!properties) { *count = mode == 2 ? 0 : mode == 3 ? 33 : 1; return VK_SUCCESS; }
    require(*count == 1 && properties[0].sType == VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR,
            "Properties must have initialized structure types");
    if (mode == 4) return VK_INCOMPLETE;
    if (mode == 6 && calls % 4 == 2) return VK_INCOMPLETE;
    if (mode == 5) { *count = 2; return VK_SUCCESS; }
    properties[0].stages = VK_SHADER_STAGE_COMPUTE_BIT;
    properties[0].subgroupSize = 32;
    std::strcpy(properties[0].name, "compute \"main\"");
    std::strcpy(properties[0].description, "driver executable");
    *count = 1;
    return VK_SUCCESS;
}
}
int main() {
    try {
        const auto device = reinterpret_cast<VkDevice>(uintptr_t{1});
        const auto pipeline = reinterpret_cast<VkPipeline>(uintptr_t{2});
        PipelineInspection unsupported(device, false, true, query);
        unsupported.inspect(pipeline);
        require(calls == 0 && !unsupported.report().enabled && !unsupported.report().supported,
                "Unsupported devices must skip the query");
        PipelineInspection disabled(device, true, false, query);
        disabled.inspect(pipeline);
        require(calls == 0 && disabled.report().supported && !disabled.report().requested,
                "Explicitly disabled inspection must skip queries");
        PipelineInspection missing(device, true, true, nullptr);
        missing.inspect(pipeline);
        require(calls == 0 && !missing.report().enabled && !missing.report().entryPointAvailable,
                "A missing device entry point must be observable");
        PipelineInspection inspector(device, true, true, query);
        inspector.inspect(pipeline);
        require(calls == 2 && inspector.report().inspected == 1 && inspector.report().failed == 0,
                "Supported pipeline must be inspected");
        const auto json = nlohmann::json::parse(inspector.jsonReport());
        require(json.at("records").at(0).at("executables").at(0).at("name") == "compute \"main\"",
                "Metadata must be bounded and JSON escaped");
        inspector.inspect(VK_NULL_HANDLE);
        require(calls == 2 && inspector.report().failed == 1, "Invalid handles must fail before querying");
        for (mode = 1; mode <= 5; ++mode) {
            const auto before = calls;
            PipelineInspection failing(device, true, true, query);
            failing.inspect(pipeline);
            require(failing.report().failed == 1 && failing.report().inspected == 0,
                    "Errors, empty, excessive and changing counts must be reported");
            require(calls - before <= 4, "Enumeration retries must be bounded");
        }
        mode = 0;
        for (unsigned i = 0; i < 100; ++i) inspector.inspect(pipeline);
        require(inspector.report().inspected == 101 && inspector.report().recordCount == 64,
                "Record limits must not prevent later pipeline inspection");
        require(nlohmann::json::parse(inspector.jsonReport()).at("records").size() == 64,
                "Report must remain bounded");
        calls = 0; mode = 6;
        PipelineInspection changing(device, true, true, query);
        changing.inspect(pipeline);
        require(calls == 4 && changing.report().inspected == 1 && changing.report().failed == 0,
                "A changing enumeration must succeed after one bounded retry");
        std::cout << "Pipeline inspection capability, errors, bounds and borrowed ownership passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
