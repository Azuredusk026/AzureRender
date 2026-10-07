#include "render/ComputePass.hpp"

#include "rhi/NullRhi.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    using namespace azurerender;
    using namespace azurerender::rhi;

    ComputePass pass({64, 33, 8, 8, 1, true});
    const ComputeDispatchSize size = pass.dispatchSize();
    assert(size.x == 8 && size.y == 5 && size.z == 1);

    NullCommandRecorder recorder;
    pass.record(recorder, reinterpret_cast<VkPipeline>(1),
                reinterpret_cast<VkPipelineLayout>(2),
                reinterpret_cast<VkDescriptorSet>(3));
    assert(recorder.calls.size() == 3);
    assert(recorder.calls[0].name == "bindComputePipeline");
    assert(recorder.calls[1].name == "bindComputeDescriptorSet");
    assert(recorder.calls[2].name == "dispatch");
    assert(recorder.calls[2].detail == "8x5x1");

    ComputePass disabled({64, 33, 8, 8, 1, false});
    const ComputeDispatchSize disabledSize = disabled.dispatchSize();
    assert(disabledSize.x == 0 && disabledSize.y == 0 && disabledSize.z == 0);
    disabled.record(recorder, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    assert(recorder.calls.size() == 3);

    bool rejected = false;
    try {
        ComputePass invalid({64, 33, 0, 8, 1, true});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
