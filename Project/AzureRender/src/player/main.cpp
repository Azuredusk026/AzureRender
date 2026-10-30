#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "app/AzureRenderApp.hpp"
#include "app/CommandLine.hpp"
#include "platform/NativeArguments.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "resources/ResourceLocator.hpp"
#include "runtime/Project.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/Level.hpp"
#include "assets/GltfLoader.hpp"
#include "runtime/SceneDocument.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/GameRuntime.hpp"
int main(int argc, char** argv) {
    try {
        const auto native=azurerender::nativeArguments(argc,argv);
        argc=static_cast<int>(native.size());
        std::string projectPath, createPath;
        bool check = false, gameTemplate = false;
        std::vector<std::string> args;
        std::string runtimeReport;
        for (int i = 1; i < argc; ++i) {
            std::string arg = native[i];
            if (arg == "--project" || arg == "--create-project" || arg == "--create-game") {
                if (arg == "--create-game") gameTemplate = true;
                if (++i == argc) throw std::runtime_error("Missing value for " + arg);
                auto& path = arg == "--project" ? projectPath : createPath;
                if (!path.empty()) throw std::runtime_error("Duplicate option: " + arg);
                path = native[i];
            } else if (arg == "--runtime-report") {
                if (++i >= argc || !runtimeReport.empty()) throw std::runtime_error("Expected one runtime report path");
                runtimeReport = native[i];
            } else if (arg == "--check-project") {
                check = true;
            } else
                args.push_back(arg);
        }
        if (!createPath.empty()) {
            if (!projectPath.empty() || check || !args.empty() || !runtimeReport.empty())
                throw std::runtime_error("--create-project is a standalone command");
            (gameTemplate ? azurerender::Project::createGame : azurerender::Project::create)(std::filesystem::u8path(createPath),
                                         std::filesystem::u8path(createPath).filename().u8string());
            std::cout << "Project created: " << createPath << '\n';
            return EXIT_SUCCESS;
        }
        AzureRenderOptions initial;
        initial.projectFile = projectPath;
        initial.runtimeReportPath = runtimeReport;
        if (!projectPath.empty()) {
            initial.projectStartup = std::make_shared<const azurerender::ProjectStartup>(
                azurerender::Project::load(std::filesystem::u8path(projectPath)));
            const auto& project = initial.projectStartup->project();
            const auto& scene = initial.projectStartup->level().scene;
            initial.renderSettings = scene.renderSettings;
            initial.sceneDocument = scene;
            if (!initial.sceneDocument->resources.empty())
                initial.assetPath = initial.sceneDocument->resources.front().path.u8string();
            std::cout << "Project loaded: " << project.name << " (" << project.id << ")\n";
        }
        auto cli = azurerender::parseCommandLine(args, std::move(initial));
        if (!cli.editorScenePath.empty() || cli.options.editorMode || !cli.createScenePath.empty())
            throw std::runtime_error(
                "Player accepts runtime scene options; editor commands belong to AzureRender");
        if (cli.showHelp) {
            std::cout << "AzurePlayer --project <project.azureproject> [runtime "
                         "options]\nAzurePlayer --create-project <empty-directory>\nAzurePlayer --create-game <empty-directory>\nAzurePlayer "
                         "--project <file> --check-project\n"
                      << "P: pause/resume runtime, O: advance one paused frame\n"
                      << "--runtime-report <file>: write final World snapshot\n";
            std::istringstream help(azurerender::commandLineHelp());
            for (std::string line; std::getline(help, line);)
                if (line.find("--editor") == std::string::npos &&
                    line.find("--create-scene") == std::string::npos)
                    std::cout << line << '\n';
            return EXIT_SUCCESS;
        }
        if (cli.showVersion) {
            std::cout << "AzurePlayer " AZURERENDER_VERSION "\n";
            return EXIT_SUCCESS;
        }
        if (check) {
            if (projectPath.empty()) throw std::runtime_error("--check-project requires --project");
            const auto& startup=*cli.options.projectStartup;
            const auto& project=startup.project();
            auto assets=startup.assets();
            for (const auto& item : assets.records()) {
                const auto& path = item.second.path;
                if(path.extension()==".azurelevel")static_cast<void>(azurerender::Level::load(path,assets));
                if(path.extension()==".gltf" || path.extension()==".glb")static_cast<void>(loadGltfAsset(path.u8string()));
            }
            azurerender::RuntimeLifecycle runtime;runtime.loadScene(startup.level().scene);runtime.start();
            azurerender::GameRuntime game(runtime,azurerender::application::systems(),azurerender::application::configuration(project));
            auto scripts=azurerender::application::scripts(project,runtime,game,assets);scripts->shutdown();
            std::cout << "Project validation passed\n";
            return EXIT_SUCCESS;
        }
        if (cli.checkResources) {
            const azurerender::ResourceLocator locator(std::filesystem::u8path(cli.options.resourceRoot));
            std::cout << locator.shaderDirectory() << '\n'
                      << locator.publicAsset("test_model.gltf") << '\n';
            return EXIT_SUCCESS;
        }
        if (!cli.scenePath.empty()) {
            auto scene = azurerender::SceneDocument::load(std::filesystem::u8path(cli.scenePath));
            auto sceneOptions = cli.options;
            sceneOptions.renderSettings = scene.renderSettings;
            cli = azurerender::parseCommandLine(args, std::move(sceneOptions));
            if (!scene.resources.empty())
                cli.options.assetPath = scene.resources.front().path.u8string();
            cli.options.sceneDocument = std::move(scene);
        }
        if (projectPath.empty() && cli.scenePath.empty())
            throw std::runtime_error("Player requires --project or --scene");
        azurerender::RuntimeDiagnostics::instance().configure("captures/azureplayer.log.jsonl");
        AzureRenderApp app;
        app.run(cli.options);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "AzurePlayer: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
