#include "rhi/NullRhi.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderSettings.hpp"
#include "scenes/CharacterSceneRenderer.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
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
    const bool withPropResource = false) {
    NullRhi rhi;
    CharacterSceneRenderer renderer;

    RenderContext context{};
    context.allocator = &rhi.allocator();
    context.rhi = &rhi;
    context.bindlessTextures = bindless;
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
    context.scene.nodes.push_back(heroNode);
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

    renderer.onLoad(context);
    SceneFrameData frame{};
    frame.renderSettings = &settings;
    frame.swapchainWidth = 1280;
    frame.swapchainHeight = 720;
    if (cameraLooksAway) {
        frame.cameraPosition[0] = 10000.0F;
        frame.cameraPosition[1] = 100.0F;
        frame.cameraTarget[0] = 20000.0F;
    }
    renderer.updateFrame(frame);

    NullCommandRecorder recorder;
    context.commands = &recorder;
    renderer.recordScene(context);
    renderer.onUnload(context);
    return recorder.calls;
}

}  // namespace

int main() {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    // NullRhi never reads shader contents, but the renderer still opens the
    // files; dummy bytes are enough.
    const std::filesystem::path shaderDirectory =
        std::filesystem::temp_directory_path() / "azr_nullrhi_shaders";
    std::filesystem::create_directories(shaderDirectory);
    const std::vector<char> dummySpv = {'\x03', '\x02', '\x23', '\x07'};
    for (const char* name : {
             "mesh.vert.spv",
             "mesh.frag.spv",
             "mesh_bindless.frag.spv",
             "outline.vert.spv",
             "outline.frag.spv",
             "background.vert.spv",
             "background.frag.spv",
             "background_bindless.frag.spv",
             "shadow.vert.spv",
             "shadow.frag.spv",
             "shadow_bindless.frag.spv",
         }) {
        std::ofstream(shaderDirectory / name, std::ios::binary)
            .write(dummySpv.data(), 4);
    }

    const std::vector<RecordedCall> legacy = runFrame(false, shaderDirectory);
    const std::vector<RecordedCall> bindless = runFrame(true, shaderDirectory);

    // Both modes record shadow pass + main pass.
    assert(countCalls(legacy, "beginRenderPass") == 2);
    assert(countCalls(legacy, "endRenderPass") == 2);
    assert(countCalls(bindless, "beginRenderPass") == 2);
    assert(countCalls(bindless, "endRenderPass") == 2);

    // The draw count is identical; only descriptor traffic may differ.
    const std::size_t legacyDraws = countCalls(legacy, "drawIndexed");
    const std::size_t bindlessDraws = countCalls(bindless, "drawIndexed");
    assert(legacyDraws > 0);
    assert(legacyDraws == bindlessDraws);

    // Bindless binds once per pass; the fixed tables bind per primitive.
    const std::size_t legacyBinds = countCalls(legacy, "bindDescriptorSet");
    const std::size_t bindlessBinds = countCalls(bindless, "bindDescriptorSet");
    assert(bindlessBinds == 2);
    assert(legacyBinds > bindlessBinds);

    // Culling: the instance behind the camera drops all geometry draws;
    // disabling culling submits it again. Passes still begin and end.
    const std::vector<RecordedCall> culled =
        runFrame(true, shaderDirectory, true, true);
    assert(countCalls(culled, "drawIndexed") == 0);
    assert(countCalls(culled, "beginRenderPass") == 2);
    const std::vector<RecordedCall> unculled =
        runFrame(true, shaderDirectory, false, true);
    assert(countCalls(unculled, "drawIndexed") == legacyDraws);

    // A second scene resource adds its own buffer binds and instanced draws
    // without changing the hero section's structure.
    const std::vector<RecordedCall> multi =
        runFrame(true, shaderDirectory, true, false, true);
    assert(countCalls(multi, "drawIndexed") > legacyDraws);
    assert(countCalls(multi, "bindVertexBuffer") >= 2);
    return 0;
}
