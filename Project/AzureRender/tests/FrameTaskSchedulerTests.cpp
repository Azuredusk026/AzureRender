#include "render/FrameTaskScheduler.hpp"
#include "render/RenderFrameSnapshot.hpp"
#include "rhi/WorkerCommandPools.hpp"
#include "render/GpuCulling.hpp"
#include "render/RecordingWorkerPool.hpp"
#include "render/DeformedBounds.hpp"
#include "render/SceneInstanceSnapshot.hpp"
#include <atomic>
#include <array>
#include <thread>
#include <stdexcept>
#include <cassert>
#include <vector>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<azurerender::RenderFrameSnapshot>);
static_assert(!std::is_move_constructible_v<azurerender::RenderFrameSnapshot>);

int main() {
    std::vector<azurerender::scene::SceneInstance> sourceInstances(1);
    sourceInstances[0].meshKey = 7;
    const azurerender::SceneInstanceSnapshot instances(sourceInstances, {0}, {{7,0,1}}, {}, {0}, {0});
    sourceInstances[0].meshKey = 9;
    if (instances.instances[0].meshKey != 7 || instances.visibleIndices[0] != 0) return 1;
    bool invalidSnapshotRejected = false;
    try { instances.validateForRecording(); }
    catch (const std::logic_error&) { invalidSnapshotRejected = true; }
    if (!invalidSnapshotRejected) return 1;
    azurerender::RecordingBufferSet validBuffers;
    validBuffers.vertices.resize(8);
    validBuffers.indices.resize(8);
    sourceInstances[0].meshKey = 7;
    const azurerender::SceneInstanceSnapshot validInstances(sourceInstances, {0}, {}, {}, {}, {0}, {}, {}, VK_NULL_HANDLE, 0, {}, validBuffers);
    validInstances.validateForRecording();
    bool invalidSpanRejected = false;
    try {
        const azurerender::SceneInstanceSnapshot invalidSpan(sourceInstances, {0}, {{7,0,2}}, {}, {}, {0}, {}, {}, VK_NULL_HANDLE, 0, {}, validBuffers);
        invalidSpan.validateForRecording();
    } catch (const std::out_of_range&) { invalidSpanRejected = true; }
    if (!invalidSpanRejected) return 1;
    azurerender::RecordingGizmoState sourceGizmo;
    sourceGizmo.selectedPrimitive = 3;
    sourceGizmo.translation = {20, 0, 0};
    const azurerender::SceneInstanceSnapshot gizmoSnapshot({}, {}, {}, {}, {}, {}, {}, {}, VK_NULL_HANDLE, 1, sourceGizmo);
    sourceGizmo.selectedPrimitive = 9;
    sourceGizmo.translation[0] = 0;
    if (gizmoSnapshot.gizmo.selectedPrimitive != 3 || gizmoSnapshot.gizmo.translation[0] != 20) return 1;
    constexpr std::uint32_t limit = 65535U * 64U;
    if (azurerender::cullingDispatchBatchSize(limit) != limit
        || azurerender::cullingDispatchBatchSize(limit + 1) != limit
        || azurerender::cullingDispatchBatchSize(1) != 1
        || azurerender::cullingDispatchBatchSize(0) != 0) return 1;
    const auto movedBounds = azurerender::includeTransformedBounds(
        {{-1, -1, -1}, {1, 1, 1}},
        {2,0,0,0, 0,1,0,0, 0,0,1,0, 20,0,0,1});
    if (movedBounds.minimum[0] != -1 || movedBounds.maximum[0] != 22) return 1;
    const auto expanded = azurerender::expandBounds({{-1,-1,-1},{1,1,1}}, 0.5F);
    if (expanded.minimum[2] != -1.5F || expanded.maximum[0] != 1.5F) return 1;
    LoadedAsset morphAsset;
    AssetMaterial displacedMaterial;
    displacedMaterial.materialFeatures = MaterialFeatureBrowOverlay;
    displacedMaterial.featureParameters[0] = 0.5F;
    displacedMaterial.styleParameters[0] = 0.001F;
    morphAsset.materials.push_back(displacedMaterial);
    if (std::abs(azurerender::materialDisplacementMargin(morphAsset) - 0.501F) > 0.00001F) return 1;
    morphAsset.boundsMin = {-1, -1, -1};
    morphAsset.boundsMax = {1, 1, 1};
    AssetVertex vertex;
    vertex.position = {1, 0, 0};
    vertex.morph0 = {4, 0, 0};
    morphAsset.vertices.push_back(vertex);
    if (azurerender::morphBounds(morphAsset, {0.5F, 0}).maximum[0] != 3.0F) return 1;
    morphAsset.hasSkin = true;
    morphAsset.jointMatrices.push_back({1,0,0,0, 0,1,0,0, 0,0,1,0, 10,0,0,1});
    auto scaledMaterialAsset = morphAsset;
    scaledMaterialAsset.jointMatrices[0][0] = 3.0F;
    if (std::abs(azurerender::materialDisplacementMargin(scaledMaterialAsset) - 1.503F) > 0.00001F) return 1;
    if (azurerender::morphBounds(morphAsset, {0.5F, 0}).maximum[0] != 13.0F) return 1;
    {
        azurerender::RecordingWorkerPool workers(4);
        std::atomic<int> executed{0};
        for (int round = 0; round < 100; ++round) {
            std::vector<azurerender::RecordingWorkerPool::Task> tasks;
            for (int i = 0; i < 16; ++i)
                tasks.push_back([&](std::size_t index) {
                    if (index >= 4) throw std::runtime_error("Invalid worker index");
                    ++executed;
                });
            workers.run(std::move(tasks));
        }
        if (executed != 1600) return 1;
        bool failed = false;
        try { workers.run({[](std::size_t) { throw std::runtime_error("failure"); }}); }
        catch (const std::runtime_error&) { failed = true; }
        if (!failed) return 1;
        workers.run({[&](std::size_t) { ++executed; }});
        if (executed != 1601) return 1;
        std::atomic<bool> nestedRejected{false};
        workers.run({[&](std::size_t) {
            try { workers.run({}); }
            catch (const std::logic_error&) { nestedRejected = true; }
        }});
        if (!nestedRejected) return 1;
    }
    azurerender::GpuCullParameters cull;
    cull.planes[0] = {1.0F, 0.0F, 0.0F, 0.0F};
    azurerender::GpuCullBounds outside{{-2.0F, -1.0F, -1.0F, 0.0F},
                                      {-1.0F, 1.0F, 1.0F, 0.0F}};
    VkDrawIndexedIndirectCommand draw{3, 1, 4, 0, 2};
    const auto hidden = azurerender::referenceCullCommand(draw, outside, cull);
    if (hidden.instanceCount != 0 || hidden.firstInstance != 2 || hidden.firstIndex != 4) return 1;
    cull.cullingEnabled = 0;
    if (azurerender::referenceCullCommand(draw, outside, cull).instanceCount != 1) return 1;
    bool invalidPoolsRejected = false;
    try { azurerender::rhi::WorkerCommandPools pools(VK_NULL_HANDLE, 0, 2, 4); }
    catch (const std::invalid_argument&) { invalidPoolsRejected = true; }
    if (!invalidPoolsRejected) return 1;
    azurerender::RenderSettings settings;
    settings.outline.strength = 0.25F;
    azurerender::SceneFrameData input;
    input.renderSettings = &settings;
    input.cameraPosition[0] = 2.0F;
    const azurerender::RenderFrameSnapshot snapshot(input);
    settings.outline.strength = 0.75F;
    input.cameraPosition[0] = 9.0F;
    if (snapshot.frame().renderSettings->outline.strength != 0.25F
        || snapshot.frame().cameraPosition[0] != 2.0F) return 1;
    azurerender::FrameTaskScheduler scheduler;
    std::vector<int> order;
    scheduler.add(2, [&] { order.push_back(2); });
    scheduler.add(1, [&] { order.push_back(1); });
    scheduler.add(2, [&] { order.push_back(3); });
    scheduler.runDeterministic();
    assert((order == std::vector<int>{1, 2, 3}));
    std::atomic<int> count{0};
    for (int i = 0; i < 64; ++i) scheduler.add(i, [&] { ++count; });
    scheduler.runParallel(4);
    if (count != 64) return 1;
    scheduler.runParallel(4);
    if (count != 64) return 1;
    scheduler.add(0, [] { throw std::runtime_error("task failure"); });
    scheduler.add(1, [&] { ++count; });
    bool failed = false;
    try { scheduler.runParallel(2); }
    catch (const std::runtime_error&) { failed = true; }
    if (!failed || count != 65) return 1;
    scheduler.runParallel(2);
    if (count != 65) return 1;
    std::array<std::atomic<int>, 4> active{};
    std::atomic<bool> overlap{false};
    for (int i = 0; i < 128; ++i) {
        scheduler.addWorkerTask(i, [&](std::size_t worker) {
            if (worker >= active.size()) { overlap = true; return; }
            if (active[worker].fetch_add(1) != 0) overlap = true;
            std::this_thread::yield();
            active[worker].fetch_sub(1);
        });
    }
    scheduler.runParallel(4);
    if (overlap) return 1;
}
