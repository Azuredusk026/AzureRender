#pragma once
#include "render/RenderContext.hpp"
#include "extensions/ISceneRenderer.hpp"

struct AzureRenderApp::EditorSceneReload {
    azurerender::RenderSettings settings;
    azurerender::RenderContext context;
    azurerender::EditorRenderResourceState resources;
    std::unique_ptr<azurerender::ISceneRenderer> renderer;
};
