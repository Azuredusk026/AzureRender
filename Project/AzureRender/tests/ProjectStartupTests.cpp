#include "runtime/LevelSession.hpp"
#include "render/FogVolumeData.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

using namespace azurerender;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void write(const std::filesystem::path& path, const std::string& value) {
    std::ofstream output(path); output << value; output.close();
    require(static_cast<bool>(output), "Fixture write failed");
}
template<class F> void rejects(F operation, const char* message) {
    bool rejected=false;
    try { operation(); } catch (const std::exception&) { rejected=true; }
    require(rejected, message);
}
}
int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("azure-startup-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Project::create(root, "Startup reuse");
        auto project = Project::load(root / "project.azureproject");
        FogVolumeData fog; fog.dimensions={1,1,1}; fog.rgba16f={0,0,0,0x3c00};
        fog.save(root / "assets/view.azfog");
        auto scene = SceneDocument::load(root / "assets/startup.azscene");
        scene.viewFog.enabled=true; scene.viewFog.asset="assets:/view.azfog";
        scene.viewFog.captureView=CameraViewDescriptor{}; scene.viewFog.projection.extent={1280,720};
        scene.save(root / "assets/startup.azscene");
        AssetDatabase assets(project); assets.refresh();
        // A prepared resolver must not start a second inventory transaction.
        // The unrelated malformed file makes a hidden fresh scan observable.
        write(root / "assets/later.azurelevel", "broken JSON after snapshot");
        auto loaded = project.loadScene(root / "assets/startup.azscene", assets);
        require(loaded.viewFog.asset == assets.resolveReference("assets:/view.azfog").u8string(),
            "Prepared scene must resolve its fog without rescanning the project");
        rejects([&]{static_cast<void>(project.loadScene(root / "assets/startup.azscene"));},
            "Standalone loading must retain fresh inventory validation");
        AssetDatabase empty(project);
        rejects([&]{static_cast<void>(project.loadScene(root / "assets/startup.azscene", empty));},
            "An unrefreshed resolver must be rejected");
        auto foreign=project;
        foreign.id="another-project";
        rejects([&]{static_cast<void>(foreign.loadScene(root / "assets/startup.azscene", assets));},
            "An index from another project identity must be rejected");
        foreign=project; foreign.file=root / "another.azureproject";
        rejects([&]{static_cast<void>(foreign.loadScene(root / "assets/startup.azscene", assets));},
            "An index from another project file must be rejected");
        foreign=project; foreign.mounts["assets"]=root;
        rejects([&]{static_cast<void>(foreign.loadScene(root / "assets/startup.azscene", assets));},
            "An index with different mount roots must be rejected");
        std::filesystem::remove(root / "assets/later.azurelevel");
        ProjectStartup legacy(project);
        require(legacy.level().scene.viewFog.asset==loaded.viewFog.asset,
            "Legacy startup must use the prepared fog resolver");

        // The second workflow uses a component-bearing level, without a mesh.
        auto level=nlohmann::json::parse(R"({
            "schemaVersion":1,"id":"prepared-level","sceneType":"sample",
            "resources":[],"prefabs":[],"nodes":[{"id":"node","components":{
                "azure.transform":{"type":"azure.transform","version":1,"data":{
                    "translation":[7,8,9],"rotation":[0,0,0],"scale":[1,1,1]}}
            }}]
        })");
        write(root / "assets/start.azurelevel", level.dump());
        project.startupScene="assets:/start.azurelevel";
        const ProjectStartup startup(project);
        static_assert(std::is_same_v<decltype(startup.assets()), const AssetDatabase&>);
        static_assert(std::is_same_v<decltype(startup.level()), const Level&>);
        require(startup.assets().version()==1 && startup.level().scene.sceneId=="prepared-level",
            "Startup must publish one complete inventory and level");
        write(root / "assets/start.azurelevel", "damaged after preparation");
        rejects([&]{ProjectStartup current(project);}, "Preparation must validate current source files");
        RuntimeLifecycle first, second; first.start(); second.start();
        LevelSession a(startup, first), b(startup, second);
        require(a.revision()==1 && a.currentReference()==project.startupScene,
            "Prepared startup must make exactly one initial commit");
        auto* transform=first.world().tryGet<ecs::TransformComponent>(first.entity("node"));
        require(transform && transform->translation==std::array<float,3>{7,8,9},
            "Prepared startup must install level components");
        transform->translation[0]=42;
        require(second.snapshotScene().nodes.at(0).translation[0]==7 &&
            startup.level().scene.nodes.at(0).translation[0]==7,
            "Runtime edits must remain local to their session");
        level["id"]="fresh-level";
        level["nodes"][0]["components"]["azure.transform"]["data"]["translation"]={11,12,13};
        write(root / "assets/start.azurelevel", level.dump());
        write(root / "assets/next.azurelevel", level.dump());
        a.request("assets:/next.azurelevel");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do { a.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while(a.loading() && std::chrono::steady_clock::now()<deadline);
        require(!a.loading() && a.lastError().empty() && a.current().scene.sceneId=="fresh-level" &&
            first.snapshotScene().nodes.at(0).translation[0]==11,
            "A subsequent request must scan and commit fresh source content");
        require(startup.level().scene.sceneId=="prepared-level" && b.current().scene.sceneId=="prepared-level",
            "Refreshing one session must preserve the shared startup and second session");
        first.stop(); second.stop();
        std::filesystem::remove_all(root);
        std::cout << "Startup context reuse passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(root); return 1;
    }
}
