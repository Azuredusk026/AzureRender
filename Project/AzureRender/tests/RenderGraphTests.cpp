#include "render/RenderGraph.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    azurerender::RenderGraph graph;
    const auto source = graph.addResource("source");
    const auto history = graph.addResource("history");
    const auto trace = graph.addPass("trace");
    const auto taa = graph.addPass("taa");
    graph.write(trace, source);
    graph.read(taa, source);
    graph.write(taa, history);
    graph.use(trace, source, azurerender::RenderGraphUsage::ColorAttachment, true);
    graph.use(taa, source, azurerender::RenderGraphUsage::Sampled, false);
    std::string error;
    assert(graph.compile(error));
    assert(error.empty());
    assert((graph.executionOrder() == std::vector<azurerender::RenderGraph::PassId>{trace, taa}));
    assert(graph.barriers().size() == 2);
    assert(graph.barriers()[0].image.newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto cycleA = graph.addPass("cycle-a");
    const auto cycleB = graph.addPass("cycle-b");
    graph.write(cycleA, history);
    graph.read(cycleA, history);
    graph.write(cycleB, source);
    graph.read(cycleB, source);
    assert(graph.compile(error));
    bool threw = false;
    try { graph.read(99, source); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
}
