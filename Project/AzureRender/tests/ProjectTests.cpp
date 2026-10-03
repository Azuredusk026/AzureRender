#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "runtime/Project.hpp"
using azurerender::Project;
void require(bool value) {
    if (!value) throw std::runtime_error("Project assertion failed");
}
template <class F>
void rejects(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    require(caught);
}
int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("azure-project-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Project::create(root, "Test Project");
        auto project = Project::load(root / "project.azureproject");
        require(project.loadStartupScene().renderSettings.sceneType ==
                azurerender::SceneType::Sample);
        require(project.loadStartupScene().resources.size() == 1);
        require(project.name == "Test Project");
        require(!project.id.empty());
        require(project.resolve("assets:/startup.azscene") ==
                std::filesystem::weakly_canonical(root / "assets/startup.azscene"));
        rejects([&] { (void)project.resolve("assets:/../escape"); });
        rejects([&] { (void)project.resolve("assets:/C:/escape"); });
        rejects([&] { (void)project.resolve("unknown:/file"); });
        rejects([&] { Project::create(root, "overwrite"); });
        const auto moved = root.string() + "-moved";
        std::filesystem::rename(root, moved);
        project = Project::load(std::filesystem::path(moved) / "project.azureproject");
        require(project.resolve(project.startupScene) ==
                std::filesystem::weakly_canonical(std::filesystem::path(moved) /
                                                  "assets/startup.azscene"));
        auto edit = [&](const std::string& text) { std::ofstream(project.file) << text; };
        edit(R"({"schemaVersion":99})");
        rejects([&] { Project::load(project.file); });
        edit(
            R"({"schemaVersion":1,"id":"x","name":"x","mounts":[{"name":"assets","path":"assets"},{"name":"assets","path":"assets"}],"startupScene":"assets:/startup.azscene"})");
        rejects([&] { Project::load(project.file); });
        edit(
            R"({"schemaVersion":1,"id":"x","name":"x","mounts":[{"name":"assets","path":"../outside"}],"startupScene":"assets:/startup.azscene"})");
        rejects([&] { Project::load(project.file); });
        edit(R"({"schemaVersion":4294967297,"id":"x","name":"x","mounts":[{"name":"assets","path":"assets"}],"startupScene":"assets:/startup.azscene"})");
        rejects([&] { Project::load(project.file); });
        edit("broken json");
        rejects([&] { Project::load(project.file); });
        std::filesystem::remove_all(moved);
        std::cout << "Project contracts passed\n";
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << "\n";
        return 1;
    }
}
