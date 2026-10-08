#include "rhi/NullRhi.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderSettings.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "scenes/BlackholeSceneRenderer.hpp"
#include "render/GpuCullingResources.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace {

using azurerender::CharacterSceneRenderer;
using azurerender::RenderContext;
using azurerender::RenderSettings;
using azurerender::SceneFrameData;
using azurerender::rhi::NullCommandRecorder;
using azurerender::rhi::NullRhi;
using azurerender::rhi::RecordedCall;

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

// One full onLoad -> updateFrame -> recordScene cycle on the mock backend.
// Returns the recorded command sequence.
std::vector<RecordedCall> runFrame(
    const bool bindless,
    const std::filesystem::path& shaderDirectory,
    const bool cullingEnabled = true,
    const bool cameraLooksAway = false,
    const bool withPropResource = false,
    const bool computeSkinning = false,
    const float sceneOffset = 0.0F,
    const bool gizmoTranslated = false,
    const bool partitionedTransparency = false,
    const int runtimeMutation = 0,
    const VkPhysicalDeviceLimits* descriptorLimits = nullptr,
    const bool visibilityPrototype = false) {
    NullRhi rhi;
    CharacterSceneRenderer renderer;

    RenderContext context{};
    context.allocator = &rhi.allocator();
    context.rhi = &rhi;
    context.bindlessTextures = bindless;
    context.visibilityPrototype = visibilityPrototype;
    context.descriptorLimits.maxPerStageDescriptorSamplers = 4096;
    context.descriptorLimits.maxDescriptorSetSamplers = 4096;
    context.descriptorLimits.maxPerStageDescriptorSampledImages = 4096;
    context.descriptorLimits.maxDescriptorSetSampledImages = 4096;
    context.descriptorLimits.maxPerStageResources = 4096;
    if (descriptorLimits) context.descriptorLimits = *descriptorLimits;
    const std::string assetPath =
        (std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR)
         / "assets_public" / "test_model.gltf")
            .string();
    context.scene.resources.push_back({"asset-0", assetPath});
    if (withPropResource) {
        context.scene.resources.push_back({"prop-1", assetPath});
    }
    azurerender::scene::SceneNodeDesc heroNode{};
    heroNode.resourceId = "asset-0";
    heroNode.translation = {sceneOffset, 0.0F, 0.0F};
    context.scene.nodes.push_back(heroNode);
    if (partitionedTransparency)
        for (int i = 1; i < 32; ++i) context.scene.nodes.push_back(heroNode);
    if (withPropResource) {
        azurerender::scene::SceneNodeDesc propNode{};
        propNode.resourceId = "prop-1";
        propNode.translation = {5.0F, 0.0F, 0.0F};
        context.scene.nodes.push_back(propNode);
    }
    context.shaderDirectory = shaderDirectory.string();
    context.rampAtlasPath =
        (std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR)
         / "assets_public" / "toon_ramp_atlas.ppm")
            .string();
    RenderSettings settings{};
    context.renderSettings = &settings;
    context.cullingEnabled = cullingEnabled;
    context.computeSkinning = computeSkinning;
    context.gpuCulling = computeSkinning;
    context.multiDrawIndirect = computeSkinning;
    context.maxDrawIndirectCount = 2;
    if (computeSkinning) {
        std::cerr << "computeSkinning test context enabled, nodes="
                  << context.scene.nodes.size() << '\n';
    }

    renderer.onLoad(context);
    SceneFrameData frame{};
    frame.renderSettings = &settings;
    frame.swapchainWidth = 1280;
    frame.swapchainHeight = 720;
    frame.cameraPosition[0] += sceneOffset;
    frame.cameraTarget[0] += sceneOffset;
    if (gizmoTranslated) {
        frame.gizmoActive = true;
        frame.selectedPrimitiveIndex = 0;
        frame.gizmoTranslation[0] = 20.0F;
        frame.gizmoRotation[1] = 45.0F;
        frame.gizmoScale[0] = 2.0F;
    }
    if (cameraLooksAway) {
        frame.cameraPosition[0] = 10000.0F;
        frame.cameraPosition[1] = 100.0F;
        frame.cameraTarget[0] = 20000.0F;
    }
    if (runtimeMutation != 0) frame.cameraPosition[2] = 5.0F;
    renderer.updateFrame(frame);

    if (runtimeMutation > 0 && runtimeMutation < 4) {
        auto snapshot = std::make_shared<azurerender::scene::SceneDescription>(context.scene);
        if (runtimeMutation == 1) snapshot->nodes[0].translation[0] = 10000.0F;
        if (runtimeMutation == 2) snapshot->nodes[0].visible = false;
        if (runtimeMutation == 3) snapshot->nodes.clear();
        frame.sceneSnapshot = snapshot;
        renderer.updateFrame(frame);
    }
    if(runtimeMutation==5){
        auto snapshot=std::make_shared<azurerender::scene::SceneDescription>(context.scene);
        for(int i=1;i<32;++i){auto node=snapshot->nodes[0];node.id="spawn-"+std::to_string(i);snapshot->nodes.push_back(node);}
        frame.sceneSnapshot=snapshot;
        for(unsigned slot=0;slot<2;++slot){frame.currentFrame=slot;renderer.updateFrame(frame);}
        context.currentFrame=frame.currentFrame;
    }
    azurerender::SceneSubmissionCounters counters;
    context.submissionCounters = &counters;
    NullCommandRecorder recorder;
    context.commands = &recorder;
    azurerender::RenderGraph graph;
    azurerender::SceneGraphResources resources{
        graph.addResource("scene-color"), graph.addResource("depth"),
        graph.addResource("normal"), graph.addResource("shadow")};
    renderer.registerPasses(graph, resources, context);
    std::string error;
    if (!graph.compile(error)) throw std::runtime_error(error);
    const std::size_t expectedPasses =
        3 + (computeSkinning ? 2 : 0);
    if (graph.passes().size() != expectedPasses) {
        throw std::runtime_error("Character registered an unexpected pass count");
    }
    if (computeSkinning) {
        const auto& skin = graph.passes().front();
        // Both identities above refer to one immutable source mesh. Each
        // instance must still dispatch into its own pose/output slice.
        std::set<std::string> meshPaths;
        for (const auto& resource : context.scene.resources)
            meshPaths.insert(std::filesystem::path(resource.path).lexically_normal().generic_string());
        const auto meshCount = meshPaths.size();
        if (skin.writes.size() != 1 || skin.reads.size() != meshCount + 1
            || graph.resources().at(skin.writes.front()).name != "character-instance-skinned")
            throw std::runtime_error("Batched skinning must declare every mesh and shared joints");
    }
    if (!recorder.calls.empty()) throw std::runtime_error("Registration must not record commands");
    if(runtimeMutation==4){auto empty=std::make_shared<azurerender::scene::SceneDescription>(context.scene);
        empty->nodes.clear();frame.sceneSnapshot=empty;renderer.updateFrame(frame);}
    graph.execute(&recorder);
    if(runtimeMutation==5&&counters.visibleInstances!=32)
        throw std::runtime_error("Spawned instances must grow every in-flight pose slice and remain visible");
    if(runtimeMutation==4&&(countCalls(recorder.calls,"dispatch")!=2
        || std::none_of(recorder.calls.begin(),recorder.calls.end(),
            [](const auto& call){return call.name=="dispatch"&&call.detail=="6x2x1";})))
        throw std::runtime_error("Frozen skinning dispatches changed after preparing another frame");
    if (runtimeMutation > 0 && runtimeMutation < 4 && counters.visibleInstances != 0)
        throw std::runtime_error("Runtime snapshot did not update rendered instances: " + std::to_string(runtimeMutation));
    if (partitionedTransparency) {
        const auto main = std::find_if(graph.passes().begin(), graph.passes().end(),
            [](const auto& pass) { return pass.name == "character-main"; });
        if (main == graph.passes().end() || main->recordingChunks.size() <= 2)
            throw std::runtime_error("Large transparent workload must be partitioned");
        const auto draws = [](const auto& calls) {
            std::vector<std::string> result;
            for (const auto& call : calls)
                if (call.name == "drawIndexed" || call.name == "drawIndexedIndirect")
                    result.push_back(call.name + call.detail);
            return result;
        };
        const auto partitionedDraws = draws(recorder.calls);
        recorder.calls.clear();
        graph.execute(&recorder, {}, true);
        if (partitionedDraws != draws(recorder.calls))
            throw std::runtime_error("Transparent partitions changed drawing order or coverage");
    }
    renderer.onUnload(context);
    return recorder.calls;
}

}  // namespace

int main() {
#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "Check failed: " << #condition << '\n'; return 1; } } while (false)
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    // NullRhi never reads shader contents, but the renderer still opens the
    // files; dummy bytes are enough.
    const std::filesystem::path shaderDirectory =
        std::filesystem::temp_directory_path() / "azr_nullrhi_shaders";
    std::filesystem::create_directories(shaderDirectory);
    const std::vector<char> dummySpv = {'\x03', '\x02', '\x23', '\x07'};
    for (const char* name : {
             "blackhole.vert.spv",
             "blackhole.frag.spv",
             "blackhole_taa.frag.spv",
             "blackhole_composite.frag.spv",
             "mesh.vert.spv",
             "mesh.frag.spv",
             "mesh_bindless.frag.spv",
             "character_snapshot.frag.spv",
             "character_snapshot_bindless.frag.spv",
             "outline.vert.spv",
             "outline.frag.spv",
             "background.vert.spv",
             "background.frag.spv",
             "background_bindless.frag.spv",
             "shadow.vert.spv",
             "mesh_compute.vert.spv",
             "outline_compute.vert.spv",
             "shadow_compute.vert.spv",
             "skin.comp.spv",
             "cull_indirect.comp.spv",
             "visibility_prototype.comp.spv",
             "shadow.frag.spv",
             "shadow_bindless.frag.spv",
         }) {
        std::ofstream(shaderDirectory / name, std::ios::binary)
            .write(dummySpv.data(), 4);
    }

    {
        NullRhi rhi;
        NullCommandRecorder recorder;
        {
            azurerender::GpuCullingResources culling(rhi);
            bool uploadRejected = false;
            try { culling.upload({}, {}); }
            catch (const std::logic_error&) { uploadRejected = true; }
            if (!uploadRejected) return 9;
            culling.initialize({'\x03', '\x02', '\x23', '\x07'}, 2);
            bool invalidInstanceRejected = false;
            try { culling.upload({}, {{3, 1, 0, 0, 1}}); }
            catch (const std::invalid_argument&) { invalidInstanceRejected = true; }
            if (!invalidInstanceRejected) return 10;
            culling.upload({{{-1, -1, -1, 0}, {1, 1, 1, 0}}},
                           {{3, 1, 0, 0, 0}});
            culling.record(recorder, {});
            if (countCalls(recorder.calls, "dispatch") != 1) return 8;
            recorder.calls.clear();
        }
        recorder.drawIndexedIndirect(VK_NULL_HANDLE, 0, 0,
                                     sizeof(VkDrawIndexedIndirectCommand));
        if (!recorder.calls.empty()) return 6;
        bool invalidIndirectRejected = false;
        try {
            recorder.drawIndexedIndirect(VK_NULL_HANDLE, 0, 1,
                                         sizeof(VkDrawIndexedIndirectCommand));
        } catch (const std::invalid_argument&) {
            invalidIndirectRejected = true;
        }
        if (!invalidIndirectRejected) return 7;
        RenderContext context{};
        RenderSettings settings{};
        context.rhi = &rhi;
        context.allocator = &rhi.allocator();
        context.commands = &recorder;
        context.renderSettings = &settings;
        context.shaderDirectory = shaderDirectory.string();
        context.renderExtent = {1280,720};
        azurerender::SceneSubmissionCounters blackholeCounters;
        context.submissionCounters = &blackholeCounters;
        azurerender::BlackholeSceneRenderer renderer;
        renderer.onLoad(context);
        azurerender::RenderGraph graph;
        const azurerender::SceneGraphResources resources{
            graph.addResource("color"), graph.addResource("depth"),
            graph.addResource("normal"), graph.addResource("shadow")};
        renderer.registerPasses(graph, resources, context);
        if (graph.passes().size() != 4) return 2;
        std::string error;
        if (!graph.compile(error)) return 3;
        graph.execute(&recorder);
        if (countCalls(recorder.calls, "draw") != 3) return 4;
        if (blackholeCounters.drawCalls != 3 || blackholeCounters.pipelineBinds != 3
            || blackholeCounters.descriptorSetBinds != 3) return 5;
        renderer.onUnload(context);
    }

    const std::vector<RecordedCall> legacy = runFrame(false, shaderDirectory);
    const std::vector<RecordedCall> bindless = runFrame(true, shaderDirectory);
    std::set<std::string> cascadeViewports;
    for (const RecordedCall& recordedCall : legacy) {
        if (recordedCall.name == "setViewport"
            && recordedCall.detail.find("1024.000000x1024.000000@") == 0) {
            cascadeViewports.insert(recordedCall.detail);
        }
    }
    assert(cascadeViewports.size() == 4);
    assert(countCalls(legacy, "setScissor") == 7);

    // Both modes record shadow pass + main pass.
    assert(countCalls(legacy, "beginRenderPass") == 3);
    assert(countCalls(legacy, "endRenderPass") == 3);
    assert(countCalls(bindless, "beginRenderPass") == 3);
    assert(countCalls(bindless, "endRenderPass") == 3);

    // The draw count is identical; only descriptor traffic may differ.
    const std::size_t legacyDraws = countCalls(legacy, "drawIndexed");
    const std::size_t bindlessDraws = countCalls(bindless, "drawIndexed");
    assert(legacyDraws > 0);
    assert(legacyDraws == bindlessDraws);

    // Moving both camera and model must preserve shadow caster submission.
    // This location lies outside the former fixed origin-centered frustum.
    const auto offsetCalls = runFrame(true, shaderDirectory, true, false,
                                     false, false, 20.0F);
    const auto shadowDrawCount = [](const std::vector<RecordedCall>& calls) {
        std::size_t passes = 0;
        std::size_t draws = 0;
        for (const auto& call : calls) {
            if (call.name == "beginRenderPass") ++passes;
            if (passes == 1 && call.name == "drawIndexed") ++draws;
        }
        return draws;
    };
    if (shadowDrawCount(bindless) == 0
        || shadowDrawCount(offsetCalls) != shadowDrawCount(bindless)) return 5;
    const auto gizmoCalls = runFrame(true, shaderDirectory, true, false,
                                     false, false, 0.0F, true);
    if (countCalls(gizmoCalls, "drawIndexed") == 0) return 11;

    // Bindless binds once per pass; the fixed tables bind per primitive.
    const std::size_t legacyBinds = countCalls(legacy, "bindDescriptorSet");
    const std::size_t bindlessBinds = countCalls(bindless, "bindDescriptorSet");
    assert(bindlessBinds == 4);
    assert(legacyBinds > bindlessBinds);

    // Four loaded materials require 35 textures, two snapshot samplers and four other fragment
    // resources. Each independent Vulkan limit must select a legal path.
    VkPhysicalDeviceLimits limits{};
    limits.maxPerStageDescriptorSamplers = 37;
    limits.maxDescriptorSetSamplers = 37;
    limits.maxPerStageDescriptorSampledImages = 37;
    limits.maxDescriptorSetSampledImages = 37;
    limits.maxPerStageResources = 41;
    const auto limitedFrame = [&](const VkPhysicalDeviceLimits& value) {
        return runFrame(true, shaderDirectory, true, false, false, false,
                        0, false, false, 0, &value);
    };
    assert(countCalls(limitedFrame(limits), "bindDescriptorSet") == bindlessBinds);
    for (auto member : {&VkPhysicalDeviceLimits::maxPerStageDescriptorSamplers,
                        &VkPhysicalDeviceLimits::maxDescriptorSetSamplers,
                        &VkPhysicalDeviceLimits::maxPerStageDescriptorSampledImages,
                        &VkPhysicalDeviceLimits::maxDescriptorSetSampledImages,
                        &VkPhysicalDeviceLimits::maxPerStageResources}) {
        auto reduced = limits;
        --(reduced.*member);
        const auto fallback = limitedFrame(reduced);
        assert(countCalls(fallback, "bindDescriptorSet") == legacyBinds);
        assert(countCalls(fallback, "drawIndexed") == legacyDraws);
        assert(shadowDrawCount(fallback) == shadowDrawCount(legacy));
    }
    assert(countCalls(limitedFrame({}), "bindDescriptorSet") == legacyBinds);

    // A distant camera looking away excludes the model from both its main
    // frustum and the camera-relative shadow cascades. Disabling culling
    // submits geometry again.
    const std::vector<RecordedCall> culled =
        runFrame(true, shaderDirectory, true, true);
    const std::size_t culledDraws = countCalls(culled, "drawIndexed");
    assert(culledDraws == 0);
    assert(culledDraws < legacyDraws);
    assert(countCalls(culled, "beginRenderPass") == 3);
    const std::vector<RecordedCall> unculled =
        runFrame(true, shaderDirectory, false, true);
    assert(countCalls(unculled, "drawIndexed") == legacyDraws);

    // Deformation is required by the union of main and shadow views.
    // Models outside every consuming view must not issue skinning work.
    const auto hiddenDeformation = runFrame(true, shaderDirectory, true, true, true, true);
    if (countCalls(hiddenDeformation, "dispatch") != 1) {
        std::cerr << "Invisible non-casters still issue compute skinning work\n";
        return 20;
    }
    if (countCalls(hiddenDeformation,"drawIndexedIndirect")!=0) {
        std::cerr << "CPU-known invisible runs must not issue empty indirect main draws\n";
        return 23;
    }
    const auto batchedDeformation = runFrame(true, shaderDirectory, true, false, false, true, 0, false, true);
    const auto batch = std::find_if(batchedDeformation.begin(), batchedDeformation.end(),
        [](const auto& call) { return call.name == "dispatch" && call.detail == "6x32x1"; });
    if (batch == batchedDeformation.end() || countCalls(batchedDeformation, "dispatch") != 2) {
        std::cerr << "Contiguous pose slices must share a two-dimensional skinning dispatch\n";
        return 22;
    }
    const auto fullDeformation = runFrame(true, shaderDirectory, false, true, true, true);
    if (countCalls(fullDeformation, "dispatch") != 2
        || std::none_of(fullDeformation.begin(),fullDeformation.end(),
            [](const auto& call){return call.name=="dispatch"&&call.detail=="6x2x1";})) {
        std::cerr << "Disabling culling must restore both deformation slices\n";
        return 21;
    }

    // A second scene resource adds its own buffer binds and instanced draws
    // without changing the hero section's structure.
    std::cerr << "Starting multi-resource pass contract\n";
    std::vector<RecordedCall> multi;
    try {
        multi = runFrame(true, shaderDirectory, true, false, true);
    } catch (const std::exception& error) {
        std::cerr << "NullRHI multi-resource test failed: "
                  << error.what() << '\n';
        return 6;
    }
    if (countCalls(multi, "drawIndexed") <= legacyDraws) {
        std::cerr << "Multi-resource draw count did not increase\n";
        return 4;
    }
    if (countCalls(multi, "bindVertexBuffer") < 2) {
        std::cerr << "Multi-resource vertex buffers were not bound\n";
        return 5;
    }
    std::cerr << "Multi-resource pass contract passed\n";
    try { runFrame(true, shaderDirectory, false, false, false, false, 0.0F, false, true); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::vector<RecordedCall> computeSkinning;
    try {
        std::cerr << "Starting compute skinning pass contract\n";
        computeSkinning =
            runFrame(true, shaderDirectory, true, false, true, true);
    } catch (const std::exception& error) {
        std::cerr << "NullRHI compute skinning test failed: "
                  << error.what() << '\n';
        return 1;
    }
    const std::size_t skinDispatches = countCalls(computeSkinning, "dispatch");
    if (skinDispatches != 2
        || std::none_of(computeSkinning.begin(),computeSkinning.end(),
            [](const auto& call){return call.name=="dispatch"&&call.detail=="6x2x1";})) {
        std::cerr << "Expected one skinning batch with two slices and one culling dispatch, got "
                  << skinDispatches << '\n';
        return 2;
    }
    const std::size_t computeDraws =
        countCalls(computeSkinning, "drawIndexed")
        + countCalls(computeSkinning, "drawIndexedIndirect");
    const std::size_t multiDraws = countCalls(multi, "drawIndexed");
    if (computeDraws != multiDraws) {
        std::cerr << "Compute skinning draw count " << computeDraws
                  << " differs from vertex fallback " << multiDraws << '\n';
        return 3;
    }
    for (int mutation = 1; mutation <= 3; ++mutation) {
        try { runFrame(true, shaderDirectory, true, false, false, false, 0.0F, false, false, mutation); }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 10; }
    }
    try {for(bool bindlessPath:{false,true})for(bool computePath:{false,true})
        runFrame(bindlessPath,shaderDirectory,false,false,false,computePath,0,false,false,5);}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 18;}
    try {runFrame(true,shaderDirectory,true,false,true,true,0,false,false,4);}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 11;}
    try {
        for(bool indexed:{false,true}) for(int mutation:{0,1,2,3,4,5}) {
            const bool multiple=mutation==0 || mutation==4;
            const auto prototype=runFrame(indexed,shaderDirectory,true,false,multiple,true,0,false,false,mutation,nullptr,true);
            const auto normal=runFrame(indexed,shaderDirectory,true,false,multiple,true,0,false,false,mutation);
            if(mutation==0 && countCalls(prototype,"bufferBarrier")<countCalls(normal,"bufferBarrier")+2)
                throw std::runtime_error("Visibility surface buffers were not scheduled through RenderGraph");
            for(const char* operation:{"drawIndexed","drawIndexedIndirect","dispatch"})
                if(countCalls(prototype,operation)!=countCalls(normal,operation))
                    throw std::runtime_error("Visibility prototype changed draw or dispatch behavior");
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 19;}
    return 0;
}
