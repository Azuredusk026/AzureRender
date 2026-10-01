#include "render/RenderGraph.hpp"
#include "rhi/NullRhi.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace azurerender;
        {
            RenderGraph chunks;
            rhi::RenderPassBeginDesc description{};
            std::vector<int> order;
            const auto pass = chunks.addGraphicsPass("chunks", description,
                [&](rhi::ICommandRecorder&) { order.push_back(99); });
            chunks.setRecordingChunks(pass, {
                [&](rhi::ICommandRecorder&) { order.push_back(1); },
                [&](rhi::ICommandRecorder&) { order.push_back(2); }});
            std::string error;
            check(chunks.compile(error), "chunk graph compiles");
            rhi::NullCommandRecorder recorder;
            chunks.execute(&recorder);
            check(order == std::vector<int>{1, 2}, "chunks replace full callback in stable order");
        }
        {
            RenderGraph commands;
            rhi::NullCommandRecorder recorder;
            commands.addCommandPass("draw", [](rhi::ICommandRecorder& output) {
                output.draw(3);
            });
            std::string diagnostic;
            check(commands.compile(diagnostic), "command graph compiles");
            bool rejected = false;
            try { commands.execute(); }
            catch (const std::logic_error&) { rejected = true; }
            check(rejected, "command graph requires recorder");
            commands.execute(&recorder);
            check(recorder.calls.size() == 1 && recorder.calls[0].name == "draw",
                  "command callback uses supplied recorder");
            bool executedRecorded = false;
            commands.execute(&recorder, [&](RenderGraph::PassId) {
                executedRecorded = true;
                return true;
            });
            check(executedRecorded && recorder.calls.size() == 1,
                  "recorded pass replaces inline recording exactly once");
        }
        RenderGraph graph;
        const auto source = graph.addResource("source");
        const auto trace = graph.addPass("trace");
        const auto taa = graph.addPass("taa");
        graph.use(trace, source, RenderGraphUsage::ColorAttachment, true);
        graph.use(taa, source, RenderGraphUsage::Sampled, false);
        std::string error = "stale diagnostic";
        check(graph.compile(error), "compile trace and taa");
        check(error.empty(), "successful compilation clears stale error");
        check(graph.executionOrder() == std::vector<RenderGraph::PassId>{trace, taa}, "RAW order");
        check(graph.barriers().size() == 2, "two state transitions");
        check(graph.barriers()[0].image.srcStageMask == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, "initial stage is valid");
        check(graph.barriers()[1].image.oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "track old layout");
        check(graph.barriers()[1].image.srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "track previous writes");
        for (const auto& barrier : graph.barriers()) rhi::validateImageBarrier(barrier.image);
        check(graph.compile(error) && graph.barriers().size() == 2, "recompile does not accumulate barriers");

        // A delayed reader must complete before the next writer (WAR).
        RenderGraph hazards;
        const auto color = hazards.addResource("color");
        const auto dependency = hazards.addResource("dependency");
        const auto a = hazards.addPass("produce");
        const auto b = hazards.addPass("dependency");
        const auto c = hazards.addPass("read");
        const auto d = hazards.addPass("overwrite");
        const auto e = hazards.addPass("consume latest");
        hazards.write(a, color);
        hazards.read(b, color);
        hazards.write(b, dependency);
        hazards.read(c, color);
        hazards.read(c, dependency);
        hazards.write(d, color);
        hazards.read(e, color);
        check(hazards.compile(error), "sequential writers are valid");
        check(hazards.executionOrder() == std::vector<RenderGraph::PassId>{a,b,c,d,e}, "WAR and latest-writer dependencies");

        RenderGraph duplicate;
        const auto target = duplicate.addResource("target");
        const auto pass = duplicate.addPass("pass");
        duplicate.use(pass, target, RenderGraphUsage::ColorAttachment, true);
        duplicate.use(pass, target, RenderGraphUsage::Sampled, false);
        check(!duplicate.compile(error), "conflicting same-pass resource states rejected");
        check(error.find("target") != std::string::npos, "diagnostic identifies resource");
        check(duplicate.executionOrder().empty() && duplicate.barriers().empty(), "failure discards compiled output");
        RenderGraph executable;
        std::vector<int> recorded;
        const auto consumer = executable.addPass("consumer", [&] { recorded.push_back(2); });
        const auto producer = executable.addPass("producer", [&] { recorded.push_back(1); });
        executable.dependsOn(consumer, producer);
        check(executable.compile(error), "explicit dependency compiles");
        executable.execute();
        check(recorded == std::vector<int>{1,2}, "callbacks follow compiled order");
        executable.dependsOn(producer, consumer);
        check(!executable.compile(error), "cycle rejected");
        bool rejected = false;
        try { executable.execute(); } catch (const std::logic_error&) { rejected = true; }
        check(rejected && recorded.size() == 2, "failed graph cannot execute callbacks");

        RenderGraph transfer;
        rhi::ImageBarrierDesc initial{};
        initial.image = reinterpret_cast<VkImage>(static_cast<std::uintptr_t>(42));
        initial.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        initial.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        initial.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        const auto output = transfer.importImage("swapchain", initial);
        rhi::NullCommandRecorder recorder;
        const auto capture = transfer.addPass("capture", [&] { recorder.draw(3); });
        transfer.use(capture, output, RenderGraphUsage::TransferSrc, false);
        const auto present = transfer.addPass("present");
        transfer.use(present, output, RenderGraphUsage::Present, false);
        transfer.dependsOn(present, capture);
        check(transfer.compile(error), "imported image graph compiles");
        check(transfer.barriers()[1].image.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            && transfer.barriers()[1].image.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            "restore imported swapchain layout");
        check(transfer.barriers()[0].image.image == initial.image, "barrier binds physical image");
        transfer.execute(&recorder);
        check(recorder.calls.size() == 3 && recorder.calls[0].name == "imageBarrier"
            && recorder.calls[1].name == "draw", "barrier executes before callback");
        transfer.addPass("late mutation");
        rejected = false;
        try { transfer.execute(&recorder); } catch (const std::logic_error&) { rejected = true; }
        check(rejected, "mutation invalidates compiled graph");
        RenderGraph buffers;
        rhi::BufferBarrierDesc bufferState{};
        bufferState.buffer = reinterpret_cast<VkBuffer>(static_cast<std::uintptr_t>(84));
        bufferState.size = 256;
        const auto vertices = buffers.importBuffer("skinned-vertices", bufferState);
        const auto skinning = buffers.addPass("skinning");
        const auto draw = buffers.addPass("draw");
        buffers.use(skinning, vertices, RenderGraphUsage::Storage, true);
        buffers.use(draw, vertices, RenderGraphUsage::VertexBuffer, false);
        check(buffers.compile(error), "compute to vertex buffer compiles");
        check(buffers.bufferBarriers().size() == 2, "buffer transitions use buffer barriers");
        check(buffers.barriers().empty(), "buffer produces no image layout transition");
        const auto& vertexBarrier = buffers.bufferBarriers()[1].buffer;
        check(vertexBarrier.srcStageMask == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            && vertexBarrier.srcAccessMask == VK_ACCESS_SHADER_WRITE_BIT
            && vertexBarrier.dstAccessMask == VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            "compute storage writes visible to vertex fetch");
        rhi::NullCommandRecorder bufferRecorder;
        buffers.execute(&bufferRecorder);
        check(bufferRecorder.calls.size() == 2 && bufferRecorder.calls[1].name == "bufferBarrier",
            "buffer barriers reach backend");
        RenderGraph indirect;
        const auto parameters = indirect.importBuffer("indirect-parameters", bufferState);
        const auto cull = indirect.addPass("gpu-culling");
        const auto indirectDraw = indirect.addPass("indirect-draw");
        indirect.use(cull, parameters, RenderGraphUsage::Storage, true);
        indirect.use(indirectDraw, parameters, RenderGraphUsage::IndirectBuffer, false);
        check(indirect.compile(error), "compute to indirect graph compiles");
        RenderGraph sortedIndices;
        auto hostState = bufferState;
        hostState.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
        hostState.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        const auto sorted = sortedIndices.importBuffer("sorted-indices", hostState);
        const auto transparent = sortedIndices.addPass("transparent");
        sortedIndices.use(transparent, sorted, RenderGraphUsage::IndexBuffer, false);
        check(sortedIndices.compile(error), "host sorted index graph compiles");
        check(sortedIndices.bufferBarriers()[0].buffer.srcAccessMask == VK_ACCESS_HOST_WRITE_BIT
            && sortedIndices.bufferBarriers()[0].buffer.dstAccessMask == VK_ACCESS_INDEX_READ_BIT,
            "host sorting writes visible to index fetch");
        const auto& indirectBarrier = indirect.bufferBarriers().back().buffer;
        check(indirectBarrier.srcStageMask == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            && indirectBarrier.srcAccessMask == VK_ACCESS_SHADER_WRITE_BIT
            && indirectBarrier.dstStageMask == VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
            && indirectBarrier.dstAccessMask == VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            "compute parameters visible to indirect command fetch");
        RenderGraph attachments;
        const auto hdr = attachments.importImage("hdr", initial);
        const auto opaque = attachments.addPass("opaque");
        const auto post = attachments.addPass("post");
        attachments.attachment(opaque, hdr, RenderGraphUsage::ColorAttachment,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        attachments.use(post, hdr, RenderGraphUsage::Sampled, false);
        check(attachments.compile(error), "render pass attachment graph compiles");
        check(attachments.barriers().size() == 1, "render pass owns attachment transition");
        check(attachments.barriers()[0].image.oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            && attachments.barriers()[0].image.srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            "graph tracks render pass final layout and writes");
        bool threw = false;
        try { graph.read(99, source); } catch (const std::out_of_range&) { threw = true; }
        check(threw, "invalid pass rejected");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
