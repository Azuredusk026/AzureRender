#include "editor/EditorSession.hpp"
#include "ecs/Components.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
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
    // Several independently identified emitters may share one scene node.
    {
        const auto path = uniquePath("-multiple-lights.azscene");
        auto document = azurerender::SceneDocument::fromAsset("test.gltf");
        const auto nodeId = document.nodes.front().id;
        document.lights.push_back({"first", nodeId, {1.0F, 0.0F, 0.0F}, 2.0F, 3.0F, true});
        document.lights.push_back({"second", nodeId, {0.0F, 1.0F, 0.0F}, 4.0F, 5.0F, false});
        azurerender::EditorContext lightContext(std::move(document), path);
        const auto* component = lightContext.ecs().tryGet<azurerender::ecs::LightComponent>(
            lightContext.entityForNode(0));
        if (component == nullptr || component->emitters.size() != 2
            || component->emitters[1].id != "second"
            || component->emitters[1].enabled) {
            return 1;
        }
        lightContext.save();
        const auto loaded = azurerender::SceneDocument::load(path);
        if (loaded.lights.size() != 2
            || loaded.lights[0].id != "first"
            || loaded.lights[1].id != "second"
            || loaded.lights[1].enabled
            || loaded.lights[1].intensity != 4.0F) {
            return 1;
        }
        std::filesystem::remove(path);
    }
    const auto scenePath = uniquePath(".azscene");
    const auto assetPath = uniquePath(".gltf");
    {
        std::ofstream asset(assetPath);
        asset << "asset-v1";
    }
    auto context = std::make_shared<azurerender::EditorContext>(
        azurerender::SceneDocument::fromAsset(assetPath), scenePath);
    azurerender::EditorSession session(context);
    azurerender::RenderSettings liveSettings;
    context->attachRenderSettings(liveSettings);
    context->beginEdit();
    liveSettings.outline.strength = 0.73F;
    context->markDirty();
    assert(session.saveOnClose());
    assert(!context->dirty());
    assert(std::filesystem::exists(scenePath));
    liveSettings.outline.strength = 0.12F;
    assert(session.execute(azurerender::EditorCommand::Reload));
    assert(liveSettings.outline.strength == 0.73F);

    assert(session.execute(azurerender::EditorCommand::ResetLayout));
    assert(session.consumeLayoutResetRequest());
    assert(!session.consumeLayoutResetRequest());

    // Node graph editing: add children, remove a subtree, reload.
    assert(context->scene().nodes.size() == 1);
    assert(context->scene().nodes[0].parentId.empty());
    context->addChildNode(0);
    assert(context->scene().nodes.size() == 2);
    assert(context->scene().nodes[1].parentId
        == context->scene().nodes[0].id);
    context->selectNode(1);
    context->setSelectedNodeName("Prefab Instance");
    context->setSelectedNodePrefab("prefabs/character.azprefab");
    context->setSelectedNodeInstance("hero-template");
    context->setGizmoTranslation({1.0F, 2.0F, 3.0F});
    assert(context->scene().nodes[1].translation[2] == 3.0F);
    assert(session.execute(azurerender::EditorCommand::Undo));
    assert(context->scene().nodes[1].translation[2] == 0.0F);
    assert(session.execute(azurerender::EditorCommand::Redo));
    assert(context->scene().nodes[1].translation[2] == 3.0F);
    context->addChildNode(1);
    context->addChildNode(1);
    assert(context->scene().nodes.size() == 4);
    context->removeNode(1);
    assert(context->scene().nodes.size() == 1);
    context->removeNode(0);  // root removal is a no-op
    assert(context->scene().nodes.size() == 1);

    context->markDirty();
    assert(session.execute(azurerender::EditorCommand::Save));
    assert(!context->dirty());

    const auto statuses = context->resourceStatuses();
    assert(statuses.size() == 1);
    assert(statuses[0].exists);
    assert(statuses[0].dependentNodeCount == 1);
    std::filesystem::last_write_time(
        assetPath,
        std::filesystem::last_write_time(assetPath)
            + std::chrono::seconds(2));
    assert(session.execute(azurerender::EditorCommand::ReloadAssets));
    assert(session.consumeAssetReloadRequest());
    assert(!session.consumeAssetReloadRequest());

    session.setCaptureLabel("Hero Front 01.png");
    assert(session.captureLabel() == "Hero_Front_01_png");
    assert(session.execute(azurerender::EditorCommand::Capture));
    std::string captureLabel;
    assert(session.consumeCaptureRequest(captureLabel));
    assert(captureLabel == "Hero_Front_01_png");
    assert(!session.consumeCaptureRequest(captureLabel));
    assert(std::filesystem::exists(scenePath));

    // Reload restores the persisted single-node document.
    assert(session.execute(azurerender::EditorCommand::Reload));
    assert(context->scene().nodes.size() == 1);
    assert(!context->dirty());

    const auto invalidPath = uniquePath("/missing/scene.azscene");
    auto invalidContext = std::make_shared<azurerender::EditorContext>(
        azurerender::SceneDocument::fromAsset("test.gltf"), invalidPath);
    azurerender::EditorSession invalidSession(invalidContext);
    invalidContext->markDirty();
    assert(!invalidSession.execute(azurerender::EditorCommand::Save));
    assert(invalidContext->dirty());
    assert(!invalidSession.lastError().empty());

    std::filesystem::remove(scenePath);
    std::filesystem::remove(assetPath);
    return 0;
}
