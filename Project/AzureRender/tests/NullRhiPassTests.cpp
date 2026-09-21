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
    const std::filesystem::path& shaderDirectory) {
    NullRhi rhi;
    CharacterSceneRenderer renderer;

    RenderContext context{};
    context.allocator = &rhi.allocator();
    context.rhi = &rhi;
    context.bindlessTextures = bindless;
    context.assetPath =
        (std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR)
         / "assets_public" / "test_model.gltf")
            .string();
    context.shaderDirectory = shaderDirectory.string();
    context.rampAtlasPath =
        (std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR)
         / "assets_public" / "toon_ramp_atlas.ppm")
            .string();
    RenderSettings settings{};
    context.renderSettings = &settings;

    renderer.onLoad(context);
    renderer.updateFrame(SceneFrameData{});

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
    return 0;
}
