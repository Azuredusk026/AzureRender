#include "rhi/NullRhi.hpp"

#include <cassert>
#include <cstring>
#include <string>

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
    RenderPassBeginDesc begin{};
    begin.extent = {1280, 720};
    recorder.beginRenderPass(begin);
    recorder.bindPipeline(nullptr);
    recorder.bindDescriptorSet(nullptr, nullptr);
    recorder.drawIndexed(36, 0);
    recorder.endRenderPass();

    assert(recorder.calls.size() == 5);
    assert(recorder.calls[0].name == "beginRenderPass");
    assert(recorder.calls[0].detail.find("1280x720") != std::string::npos);
    assert(recorder.calls[1].name == "bindPipeline");
    assert(recorder.calls[2].name == "bindDescriptorSet");
    assert(recorder.calls[3].name == "drawIndexed");
    assert(recorder.calls[4].name == "endRenderPass");

    // The fake allocator keeps mapped writes working and tracks statistics.
    NullGpuAllocator allocator;
    auto buffer = allocator.createBuffer(256, 0, true);
    assert(buffer.buffer != VK_NULL_HANDLE);
    assert(buffer.mapped != nullptr);
    std::memset(buffer.mapped, 0xAB, 256);
    assert(static_cast<unsigned char*>(buffer.mapped)[255] == 0xAB);
    assert(allocator.statistics().liveBuffers == 1);
    allocator.destroyBuffer(buffer);
    assert(buffer.buffer == VK_NULL_HANDLE);
    assert(allocator.statistics().liveBuffers == 0);

    auto image = allocator.createImage2D(
        64, 64, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    assert(image.image != VK_NULL_HANDLE);
    allocator.destroyImage(image);
    assert(allocator.statistics().liveImages == 0);

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
    return 0;
}
