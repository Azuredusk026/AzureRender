#include "ComputePass.hpp"

#include <stdexcept>

namespace azurerender {

ComputePass::ComputePass(ComputePassDesc desc) : desc_(desc) {
    if (desc_.localSizeX == 0 || desc_.localSizeY == 0 || desc_.localSizeZ == 0) {
        throw std::invalid_argument("Compute pass local size must be nonzero");
    }
}

ComputeDispatchSize ComputePass::dispatchSize() const noexcept {
    if (!desc_.enabled || desc_.width == 0 || desc_.height == 0) {
        return {};
    }
    return {(desc_.width + desc_.localSizeX - 1) / desc_.localSizeX,
            (desc_.height + desc_.localSizeY - 1) / desc_.localSizeY,
            (desc_.localSizeZ == 0) ? 0U : 1U};
}

void ComputePass::record(rhi::ICommandRecorder& recorder,
                         const VkPipeline pipeline,
                         const VkPipelineLayout layout,
                         const VkDescriptorSet descriptorSet) const {
    const ComputeDispatchSize groups = dispatchSize();
    if (groups.x == 0 || groups.y == 0 || groups.z == 0) {
        return;
    }
    recorder.bindComputePipeline(pipeline);
    recorder.bindComputeDescriptorSet(layout, descriptorSet);
    recorder.dispatch(groups.x, groups.y, groups.z);
}

}  // namespace azurerender
