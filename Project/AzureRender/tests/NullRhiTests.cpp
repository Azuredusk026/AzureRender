#ifdef NDEBUG
#error Null RHI contract checks require active assertions.
#endif
#include "rhi/NullRhi.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
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
    {
        using namespace azurerender::rhi;
        NullRhi backend;GraphicsPipelineDesc graphics;
        graphics.vertexSpecialization={{0,0x3f000000U}};
        graphics.fragmentSpecialization={{0,1},{7,0xffffffffU}};
        auto pipeline=backend.createGraphicsPipeline(graphics);
        if(backend.calls.back().detail.find(" fragmentSpec=0:1/7:4294967295/")==std::string::npos)return 30;
        backend.destroyPipeline(pipeline);
        graphics.fragmentSpecialization.push_back({7,0});
        const auto count=backend.calls.size();bool rejected=false;
        try{backend.createGraphicsPipeline(graphics);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||backend.calls.size()!=count)return 31;
        graphics.fragmentSpecialization.clear();graphics.vertexSpecialization.clear();
        for(unsigned i=0;i<17;++i)graphics.vertexSpecialization.push_back({i,i+1});
        rejected=false;try{backend.createGraphicsPipeline(graphics);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||backend.calls.size()!=count)return 32;
        ComputePipelineDesc compute{reinterpret_cast<VkShaderModule>(1),reinterpret_cast<VkPipelineLayout>(2)};
        for(unsigned i=0;i<16;++i)compute.specialization.push_back({i,i+5});
        pipeline=backend.createComputePipeline(compute);
        if(backend.calls.back().detail.find(" spec=0:5/1:6/")==std::string::npos)return 33;
        backend.destroyPipeline(pipeline);
        compute.specialization[15].id=0;rejected=false;
        try{backend.createComputePipeline(compute);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)return 34;
        compute.specialization.clear();pipeline=backend.createComputePipeline(compute);
        if(backend.calls.back().detail.find(" spec=")!=std::string::npos)return 35;
        backend.destroyPipeline(pipeline);
    }
    {
        NullRhi backend;azurerender::rhi::GraphicsPipelineDesc desc;
        desc.depthBiasExact=true;bool rejected=false;
        try{backend.createGraphicsPipeline(desc);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||!backend.calls.empty())return 20;
        desc.depthBias=true;backend.exactDepthBiasSupported=false;rejected=false;
        try{backend.createGraphicsPipeline(desc);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||!backend.calls.empty())return 21;
        desc.depthBiasExact=false;auto pipeline=backend.createGraphicsPipeline(desc);backend.destroyPipeline(pipeline);
        backend.exactDepthBiasSupported=true;desc.depthBiasExact=true;
        pipeline=backend.createGraphicsPipeline(desc);
        if(backend.calls.back().detail.find("exactDepthBias=1")==std::string::npos)return 22;
        backend.destroyPipeline(pipeline);
    }
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
    VkImageCreateInfo cubeInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    cubeInfo.imageType=VK_IMAGE_TYPE_2D;cubeInfo.extent={2,2,1};
    cubeInfo.mipLevels=1;cubeInfo.arrayLayers=6;cubeInfo.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    cubeInfo.format=VK_FORMAT_R8G8B8A8_UNORM;cubeInfo.usage=VK_IMAGE_USAGE_SAMPLED_BIT;
    auto cube=allocator.createImage(cubeInfo,false);
    if(allocator.statistics().liveImageBytes!=96){std::fprintf(stderr,"Cube allocations must retain all six layers\n");return 91;}
    allocator.destroyImage(cube);
    if(allocator.statistics().liveImageBytes!=0){std::fprintf(stderr,"Cube allocation must release every layer\n");return 92;}

    // The device mock mints distinct handles and logs creation.
    NullRhi rhi;
    {
        azurerender::rhi::GraphicsPipelineDesc selective;
        selective.colorAttachmentCount=2;
        selective.colorWriteMasks={0,VK_COLOR_COMPONENT_R_BIT};
        if(azurerender::rhi::graphicsColorWriteMask(selective,0)!=0
            ||azurerender::rhi::graphicsColorWriteMask(selective,1)!=VK_COLOR_COMPONENT_R_BIT)return 93;
        selective.colorWriteMasks={0};
        bool rejected=false;
        try {rhi.createGraphicsPipeline(selective);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||countCalls(rhi.calls,"createGraphicsPipeline")!=0)return 94;
        selective.colorWriteMasks={0,0x10};rejected=false;
        try {rhi.createGraphicsPipeline(selective);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||countCalls(rhi.calls,"createGraphicsPipeline")!=0)return 95;
    }
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
