#include "editor/EditorSession.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace {

std::filesystem::path uniquePath(const std::string& suffix) {
    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("azurerender-session-" + std::to_string(stamp) + suffix);
}

}  // namespace

int main() {
    const auto scenePath = uniquePath(".azscene");
    auto context = std::make_shared<azurerender::EditorContext>(
        azurerender::SceneDocument::fromAsset("test.gltf"), scenePath);
    azurerender::EditorSession session(context);
    context->markDirty();
    assert(session.saveOnClose());
    assert(!context->dirty());
    assert(std::filesystem::exists(scenePath));

    assert(session.execute(azurerender::EditorCommand::ResetLayout));
    assert(session.consumeLayoutResetRequest());
    assert(!session.consumeLayoutResetRequest());

    const auto invalidPath = uniquePath("/missing/scene.azscene");
    auto invalidContext = std::make_shared<azurerender::EditorContext>(
        azurerender::SceneDocument::fromAsset("test.gltf"), invalidPath);
    azurerender::EditorSession invalidSession(invalidContext);
    invalidContext->markDirty();
    assert(!invalidSession.execute(azurerender::EditorCommand::Save));
    assert(invalidContext->dirty());
    assert(!invalidSession.lastError().empty());

    std::filesystem::remove(scenePath);
    return 0;
}
