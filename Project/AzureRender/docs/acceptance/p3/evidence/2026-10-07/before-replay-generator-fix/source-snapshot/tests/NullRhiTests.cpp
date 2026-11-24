#include "rhi/NullRhi.hpp"

#include <cassert>
#include <cstring>
#include <string>
#include <stdexcept>

#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace {

using azurerender::rhi::NullCommandRecorder;
using azurerender::rhi::NullGpuAllocator;
using azurerender::rhi::NullRhi;
using azurerender::rhi::RecordedCall;
using azurerender::rhi::RenderPassBeginDesc;

std::size_t countCalls(
    const std::vector<RecordedCall>& calls,
    const std::string& name) {
    std::size_t count = 0;
    for (const RecordedCall& call : calls) {
        if (call.name == name) {
            ++count;
        }
    }
    return count;
}

}  // namespace

int main() {
#ifdef _WIN32
    // Asserts must print and abort, not wait on a modal dialog: the harness
    // runs tests without a visible console.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    // Recording order is the contract: a pass test reads the sequence.
    NullCommandRecorder recorder;
    azurerender::rhi::ImageBarrierDesc invalid{};
    invalid.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bool rejected = false;
    try { recorder.imageBarrier(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected || !recorder.calls.empty()) return 10;
    RenderPassBeginDesc begin{};
    begin.extent = {1280, 720};
    recorder.beginRenderPass(begin);
    recorder.bindPipeline(nullptr);
    recorder.bindDescriptorSet(nullptr, nullptr);
    recorder.drawIndexed(36, 0);
    recorder.dispatch(8, 4, 1);
    recorder.endRenderPass();
    recorder.bufferBarrier({
        VK_NULL_HANDLE,
        16,
        64,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});

    assert(recorder.calls.size() == 7);
    assert(recorder.calls[0].name == "beginRenderPass");
    assert(recorder.calls[0].detail.find("1280x720") != std::string::npos);
    assert(recorder.calls[1].name == "bindPipeline");
    assert(recorder.calls[2].name == "bindDescriptorSet");
    assert(recorder.calls[3].name == "drawIndexed");
    assert(recorder.calls[4].name == "dispatch");
    assert(recorder.calls[5].name == "endRenderPass");
    assert(recorder.calls[6].name == "bufferBarrier");
    if (recorder.calls[6].detail.find("offset=16 size=64") == std::string::npos
        || recorder.calls[6].detail.find("dstAccess=4") == std::string::npos)
        return 11;
    azurerender::rhi::ImageBarrierDesc depth{};
    depth.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depth.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    depth.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth.baseMipLevel = 2;
    depth.mipLevels = 3;
    recorder.imageBarrier(depth);
    if (recorder.calls.back().detail.find("aspect=2 baseMip=2 mipLevels=3")
        == std::string::npos) return 12;

    // The fake allocator keeps mapped writes working and tracks statistics.
    NullGpuAllocator allocator;
    auto buffer = allocator.createBuffer(256, 0, true);
    assert(buffer.buffer != VK_NULL_HANDLE);
    assert(buffer.mapped != nullptr);
    std::memset(buffer.mapped, 0xAB, 256);
    assert(static_cast<unsigned char*>(buffer.mapped)[255] == 0xAB);
    assert(allocator.statistics().liveBuffers == 1);
    assert(allocator.statistics().liveBufferBytes == 256);
    allocator.destroyBuffer(buffer);
    assert(buffer.buffer == VK_NULL_HANDLE);
    assert(allocator.statistics().liveBuffers == 0);
    assert(allocator.statistics().liveBufferBytes == 0);

    auto image = allocator.createImage2D(
        64, 64, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    assert(image.image != VK_NULL_HANDLE);
    assert(allocator.statistics().liveImageBytes == 16384);
    allocator.destroyImage(image);
    assert(allocator.statistics().liveImages == 0);
    assert(allocator.statistics().liveImageBytes == 0);

    // The device mock mints distinct handles and logs creation.
    NullRhi rhi;
    const VkPipeline first =
        rhi.createGraphicsPipeline(azurerender::rhi::GraphicsPipelineDesc{});
    const VkPipeline second =
        rhi.createGraphicsPipeline(azurerender::rhi::GraphicsPipelineDesc{});
    assert(first != second);
    assert(countCalls(rhi.calls, "createGraphicsPipeline") == 2);
    rhi.destroyPipeline(first);
    assert(countCalls(rhi.calls, "destroyPipeline") == 1);
    const auto compute = rhi.createComputePipeline({reinterpret_cast<VkShaderModule>(1), reinterpret_cast<VkPipelineLayout>(2)});
    if (!compute) return 20;
    NullCommandRecorder computeCommands;
    computeCommands.bindComputePipeline(compute);
    computeCommands.bindComputeDescriptorSet(reinterpret_cast<VkPipelineLayout>(2), reinterpret_cast<VkDescriptorSet>(3));
    computeCommands.dispatch(8,4,1);
    if (computeCommands.calls.size()!=3 || computeCommands.calls[0].name!="bindComputePipeline"
        || computeCommands.calls[1].name!="bindComputeDescriptorSet") return 21;
    rejected=false;
    try { computeCommands.dispatch(0,1,1); } catch(const std::invalid_argument&) { rejected=true; }
    if (!rejected || computeCommands.calls.size()!=3) return 22;
    return 0;
}
