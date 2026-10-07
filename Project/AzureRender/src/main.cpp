#if defined(_WIN32) && defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <crtdbg.h>
#endif
#include "app/AzureRenderApp.hpp"
#include "app/CommandLine.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorSession.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "resources/ResourceLocator.hpp"
#include "editor/SceneModel.hpp"
#include "devtools/GeometryProcessCompiler.hpp"

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
#if defined(_WIN32) && defined(_MSC_VER)
        // Automated runs report fatal errors to their caller instead of opening a modal dialog.
        if(options.smokeFrameLimit>0 || options.captureFrameLimit>0) {
            SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
            _set_abort_behavior(_WRITE_ABORT_MSG,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#ifdef _DEBUG
            for(int kind:{_CRT_WARN,_CRT_ERROR,_CRT_ASSERT}) {
                _CrtSetReportMode(kind,_CRTDBG_MODE_FILE);_CrtSetReportFile(kind,_CRTDBG_FILE_STDERR);
            }
#endif
        }
#endif
        std::string& scenePath = commandLine.scenePath;
        if (commandLine.showHelp) {
            std::cout << azurerender::commandLineHelp();
            return EXIT_SUCCESS;
        }
        if (commandLine.showVersion) {
            azurerender::RuntimeDiagnostics::instance().print(
                "cli", "AzureRender " AZURERENDER_VERSION);
            return EXIT_SUCCESS;
        }
        if (commandLine.checkResources) {
            const azurerender::ResourceLocator locator(options.resourceRoot);
            azurerender::RuntimeDiagnostics::instance().print(
                "cli",
                "Shader directory: " + locator.shaderDirectory().string() + '\n'
                + "Public demo: " + locator.publicAsset("test_model.gltf").string() + '\n'
                + "Ramp profile: " + locator.rampProfile().string() + '\n'
                + "Ramp atlas: " + locator.rampAtlas().string() + '\n'
                + "Showcase looks: " + locator.showcaseLooks().string());
            return EXIT_SUCCESS;
        }
        if (!commandLine.createGamePath.empty()) {
            const std::filesystem::path directory(commandLine.createGamePath);
            azurerender::Project::createGame(directory, directory.filename().string());
            std::cout << "Game project created: " << directory << '\n';
            return EXIT_SUCCESS;
        }
        if (!commandLine.createScenePath.empty()) {
            const azurerender::SceneDocument scene =
                azurerender::SceneDocument::fromAsset(options.assetPath);
            scene.save(commandLine.createScenePath);
            azurerender::RuntimeDiagnostics::instance().print(
                "cli", "Scene created: " + commandLine.createScenePath);
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
                options.sceneDocument = scene;
                options.editorSession =
                    std::make_shared<azurerender::EditorSession>(
                        std::make_shared<azurerender::EditorContext>(
                            std::move(scene), commandLine.editorScenePath));
            } else {
                options.sceneDocument = std::move(scene);
            }
        }

        if(options.editorMode && !options.projectFile.empty()){
            auto context=azurerender::EditorContext::openProject(options.projectFile);
            options.sceneDocument=context->scene();options.renderSettings=context->renderSettings();
            if(!context->scene().resources.empty())options.assetPath=context->scene().resources.front().path.string();
            options.editorSession=std::make_shared<azurerender::EditorSession>(std::move(context));
        }
        if(options.editorSession){
            options.editorSession->setClosePolicy(options.editorClosePolicy);
            const azurerender::ResourceLocator locator(options.resourceRoot);
            const auto name=std::filesystem::path(
#ifdef _WIN32
                "AzureGeometryCompiler.exe"
#else
                "AzureGeometryCompiler"
#endif
            );
            const auto& roots=locator.searchRoots();
            const auto compiler=std::find_if(roots.begin(),roots.end(),[&](const auto& root){return std::filesystem::is_regular_file(root/name);});
            if(compiler!=roots.end())azurerender::registerProceduralGenerators(options.editorSession->generators(),
                std::make_shared<azurerender::GeometryProcessCompiler>(*compiler/name,std::filesystem::temp_directory_path()/"AzureRender-procedural"));
            else options.editorSession->log("Procedural compiler service is unavailable");
        }
        AzureRenderApp application;
        application.run(options);
    } catch (const azurerender::CommandLineError& exception) {
        constexpr auto code = azurerender::DiagnosticCode::InvalidArguments;
        azurerender::RuntimeDiagnostics::instance().error(
            "cli", code, exception.what());
        return static_cast<int>(code);
    } catch (const std::exception& exception) {
        azurerender::RuntimeDiagnostics::instance().error(
            "main",
            static_cast<azurerender::DiagnosticCode>(
                azurerender::diagnosticExitCode(exception.what())),
            exception.what());
        return azurerender::diagnosticExitCode(exception.what());
    }

    return EXIT_SUCCESS;
}
