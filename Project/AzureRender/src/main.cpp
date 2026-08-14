#include "app/AzureRenderApp.hpp"
#include "app/CommandLine.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorSession.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "resources/ResourceLocator.hpp"
#include "editor/SceneModel.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int main(const int argumentCount, char** argumentValues) {
    azurerender::RuntimeDiagnostics::instance().configure(
        "captures/azurerender.log.jsonl");
    try {
        std::vector<std::string> arguments;
        if (argumentCount > 1) {
            arguments.reserve(static_cast<std::size_t>(argumentCount - 1));
        }
        for (int index = 1; index < argumentCount; ++index) {
            arguments.emplace_back(argumentValues[index]);
        }
        azurerender::ParsedCommandLine commandLine =
            azurerender::parseCommandLine(arguments);
        AzureRenderOptions& options = commandLine.options;
        std::string& scenePath = commandLine.scenePath;
        if (commandLine.showVersion) {
            std::cout << "AzureRender " << AZURERENDER_VERSION << '\n';
            return EXIT_SUCCESS;
        }
        if (commandLine.checkResources) {
            const azurerender::ResourceLocator locator(options.resourceRoot);
            std::cout << "Shader directory: " << locator.shaderDirectory() << '\n'
                      << "Public demo: " << locator.publicAsset("test_model.gltf") << '\n'
                      << "Ramp profile: " << locator.rampProfile() << '\n'
                      << "Ramp atlas: " << locator.rampAtlas() << '\n';
            return EXIT_SUCCESS;
        }
        if (!commandLine.createScenePath.empty()) {
            const azurerender::SceneDocument scene =
                azurerender::SceneDocument::fromAsset(options.assetPath);
            scene.save(commandLine.createScenePath);
            std::cout << "Scene created: " << commandLine.createScenePath << '\n';
            return EXIT_SUCCESS;
        }
        if (!commandLine.editorScenePath.empty()) {
            scenePath = commandLine.editorScenePath;
            options.editorMode = true;
            options.editorScenePath = commandLine.editorScenePath;
        }
        if (!scenePath.empty()) {
            azurerender::SceneDocument scene =
                azurerender::SceneDocument::load(scenePath);
            const auto asset = std::find_if(
                scene.resources.begin(), scene.resources.end(),
                [](const azurerender::SceneResource& resource) {
                    return resource.type == "gltf";
                });
            if (asset == scene.resources.end()) {
                throw std::runtime_error(
                    "Scene contains no gltf resource: " + scenePath);
            }
            options.assetPath = asset->path.string();
            options.renderSettings = scene.renderSettings;
            if (options.editorMode) {
                options.editorSession =
                    std::make_shared<azurerender::EditorSession>(
                        std::make_shared<azurerender::EditorContext>(
                            std::move(scene), commandLine.editorScenePath));
            }
        }

        AzureRenderApp application;
        application.run(options);
        if (options.editorSession != nullptr
            && !options.editorSession->saveOnClose()) {
            throw std::runtime_error(
                "Editor close save failed: "
                + options.editorSession->lastError());
        }
    } catch (const azurerender::CommandLineError& exception) {
        constexpr auto code = azurerender::DiagnosticCode::InvalidArguments;
        azurerender::RuntimeDiagnostics::instance().error(
            "cli", code, exception.what());
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return static_cast<int>(code);
    } catch (const std::exception& exception) {
        azurerender::RuntimeDiagnostics::instance().error(
            "main",
            static_cast<azurerender::DiagnosticCode>(
                azurerender::diagnosticExitCode(exception.what())),
            exception.what());
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return azurerender::diagnosticExitCode(exception.what());
    }

    return EXIT_SUCCESS;
}
