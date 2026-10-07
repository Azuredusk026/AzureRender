#include "assets/GltfLoader.hpp"
#include "render/RenderMath.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/SceneDocument.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace azurerender::internal;
    if (argc != 2) return 2;
    const auto path = std::filesystem::temp_directory_path() /
        ("azure-mirrored-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".gltf");
    try {
        nlohmann::json document;
        std::ifstream(argv[1]) >> document;
        document["nodes"][0]["scale"] = {-1, 2, 3};
        { std::ofstream output(path); output << document; }
        const auto asset = loadGltfAsset(path.string());
        unsigned triangles = 0;
        for (std::size_t i=0; i+2<asset.indices.size(); i+=3) {
            const auto& a=asset.vertices.at(asset.indices[i]);
            const auto& b=asset.vertices.at(asset.indices[i+1]);
            const auto& c=asset.vertices.at(asset.indices[i+2]);
            const auto face=cross(subtract(b.position,a.position),subtract(c.position,a.position));
            if (dot(face,a.normal)<=0) throw std::runtime_error("Baked mirror triangle winding opposes its outward normal");
            ++triangles;
        }
        if (triangles!=12) throw std::runtime_error("Cube fixture triangle count");
        azurerender::RenderSettings settings;
        azurerender::decodeLevelRenderSettings(settings, {{"cameraNear",.2},{"cameraFar",500},{"shadowDistance",80}});
        const auto encoded=azurerender::encodeLevelRenderSettings(settings);
        if (encoded.at("cameraNear")!=.2F || encoded.at("cameraFar")!=500 || encoded.at("shadowDistance")!=80)
            throw std::runtime_error("Authored camera and shadow range were not retained");
        for (const auto& invalid : {nlohmann::json{{"cameraNear",0}}, nlohmann::json{{"cameraFar",.1}},
                nlohmann::json{{"cameraFar",5001}}, nlohmann::json{{"shadowDistance",501}}}) {
            bool rejected=false;
            try { azurerender::decodeLevelRenderSettings(settings,invalid); }
            catch(const std::invalid_argument&) { rejected=true; }
            if (!rejected || azurerender::encodeLevelRenderSettings(settings)!=encoded)
                throw std::runtime_error("Invalid camera range must retain active settings");
        }
        auto scene=azurerender::SceneDocument::fromAsset(argv[1]);scene.renderSettings=settings;
        auto scenePath=path;scenePath.replace_extension(".azscene");scene.save(scenePath);
        const auto reopened=azurerender::SceneDocument::load(scenePath);
        std::filesystem::remove(scenePath);
        if (azurerender::encodeLevelRenderSettings(reopened.renderSettings)!=encoded)
            throw std::runtime_error("Scene camera range roundtrip");
        std::filesystem::remove(path);
        std::cout << "Baked mirror retains twelve outward-facing triangles\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove(path);
        return 1;
    }
}
