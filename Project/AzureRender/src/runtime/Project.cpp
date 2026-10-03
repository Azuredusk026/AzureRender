#include "Project.hpp"
#include "runtime/Level.hpp"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

#include "SceneDocument.hpp"
#include "resources/ResourceLocator.hpp"
namespace azurerender {
namespace {
std::filesystem::path inside(const std::filesystem::path& root, const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name())
        throw std::runtime_error("Project path must be relative: " + path.string());
    const auto base = std::filesystem::weakly_canonical(root);
    const auto result = std::filesystem::weakly_canonical(base / path);
    const auto relative = result.lexically_relative(base);
    if (relative.empty() || *relative.begin() == "..")
        throw std::runtime_error("Project path escapes mount: " + path.string());
    return result;
}
bool mountName(const std::string& name) {
    if (name.empty() || name == "engine") return false;
    for (const unsigned char c : name)
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_') return false;
    return true;
}
std::string uuid() {
    std::random_device random;
    unsigned char bytes[16];
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 15) | 64);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 63) | 128);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out << '-';
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}
}  // namespace
void Project::create(const std::filesystem::path& directory, const std::string& projectName) {
    if (projectName.empty()) throw std::runtime_error("Project name must be nonempty");
    if (std::filesystem::exists(directory) && !std::filesystem::is_empty(directory))
        throw std::runtime_error("Project directory must be empty: " + directory.string());
    std::filesystem::create_directories(directory / "assets");
    SceneDocument scene = SceneDocument::fromAsset("engine:/assets_public/test_model.gltf");
    scene.sceneId = "startup";
    scene.renderSettings.sceneType = SceneType::Sample;
    scene.save(directory / "assets/startup.azscene");
    const nlohmann::json document = {
        {"schemaVersion", kSchemaVersion},
        {"id", uuid()},
        {"name", projectName},
        {"mounts", nlohmann::json::array({{{"name", "assets"}, {"path", "assets"}}})},
        {"startupScene", "assets:/startup.azscene"}};
    std::ofstream output(directory / "project.azureproject");
    output << document.dump(2) << '\n';
    output.flush();
    if (!output) throw std::runtime_error("Cannot write project: " + directory.string());
}
void Project::createGame(const std::filesystem::path& directory, const std::string& projectName) {
    if (projectName.empty()) throw std::runtime_error("Project name must be nonempty");
    const auto target = std::filesystem::absolute(directory).lexically_normal();
    if (std::filesystem::exists(target) && !std::filesystem::is_empty(target))
        throw std::runtime_error("Project directory must be empty: " + target.string());
    const auto candidate = target.parent_path() / (".azure-template-" + uuid());
    try {
        const auto source = ResourceLocator().publicAsset("gameplay");
        std::filesystem::create_directories(candidate / "assets");
        for (auto it = std::filesystem::recursive_directory_iterator(source / "assets");
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (it->path().filename() == ".azure") { it.disable_recursion_pending(); continue; }
            if (!it->is_regular_file() || it->path().extension() == ".azmeta" || it->path().extension() == ".tmp") continue;
            const auto output = candidate / "assets" / it->path().lexically_relative(source / "assets");
            std::filesystem::create_directories(output.parent_path());
            std::filesystem::copy_file(it->path(), output);
        }
        std::ifstream input(source / "project.azureproject");
        nlohmann::json document; input >> document;
        document["id"] = uuid(); document["name"] = projectName;
        { std::ofstream output(candidate / "project.azureproject"); output << document.dump(2) << '\n';
          output.flush(); if (!output) throw std::runtime_error("Cannot write game project"); }
        static_cast<void>(Project::load(candidate / "project.azureproject").loadStartupScene());
        std::filesystem::remove_all(candidate / ".azure");
        if (std::filesystem::exists(target)) std::filesystem::remove(target);
        std::filesystem::rename(candidate, target);
    } catch (...) { std::filesystem::remove_all(candidate); throw; }
}
Project Project::load(const std::filesystem::path& path) {
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot open project");
        nlohmann::json document;
        input >> document;
        if (!document.at("schemaVersion").is_number_integer() ||
            document.at("schemaVersion") != kSchemaVersion)
            throw std::runtime_error("Unsupported project schemaVersion");
        Project project;
        project.file = std::filesystem::weakly_canonical(path);
        project.id = document.at("id").get<std::string>();
        project.name = document.at("name").get<std::string>();
        if (project.id.empty() || project.name.empty())
            throw std::runtime_error("Project id and name must be nonempty");
        const auto& mounts = document.at("mounts");
        if (!mounts.is_array() || mounts.empty())
            throw std::runtime_error("Project mounts must be a nonempty array");
        for (const auto& entry : mounts) {
            const auto name = entry.at("name").get<std::string>();
            if (!mountName(name))
                throw std::runtime_error("Invalid or reserved mount name: " + name);
            const auto root =
                inside(project.file.parent_path(), entry.at("path").get<std::string>());
            if (!std::filesystem::is_directory(root))
                throw std::runtime_error("Mount directory missing: " + root.string());
            if (!project.mounts.emplace(name, root).second)
                throw std::runtime_error("Duplicate project mount: " + name);
        }
        project.startupScene = document.at("startupScene").get<std::string>();
        if (!std::filesystem::is_regular_file(project.resolve(project.startupScene)))
            throw std::runtime_error("startupScene missing");
        return project;
    } catch (const std::exception& error) {
        throw std::runtime_error("Project '" + path.string() + "': " + error.what());
    }
}
SceneDocument Project::loadStartupScene() const {
    const auto sceneFile = resolve(startupScene);
    if (sceneFile.extension() == ".azurelevel") { AssetDatabase assets(*this); assets.refresh(); return Level::load(sceneFile, assets).scene; }
    auto scene = SceneDocument::load(sceneFile);
    for (auto& resource : scene.resources) {
        const auto path = resource.path.generic_string();
        if (path.find(":/") != std::string::npos)
            resource.path = resolve(path);
        else {
            if (resource.path.is_absolute())
                throw std::runtime_error(
                    "Project scene resource must use a virtual or relative path: " + path);
            resource.path = inside(
                file.parent_path(),
                (sceneFile.parent_path() / resource.path).lexically_relative(file.parent_path()));
        }
        if (!std::filesystem::is_regular_file(resource.path))
            throw std::runtime_error("Scene resource missing: " + resource.path.string());
    }
    return scene;
}
std::filesystem::path Project::resolve(const std::string& virtualPath) const {
    const auto separator = virtualPath.find(":/");
    if (separator == std::string::npos)
        throw std::runtime_error("Expected mount:/relative path: " + virtualPath);
    const auto mount = virtualPath.substr(0, separator);
    const auto relative = virtualPath.substr(separator + 2);
    // Reject platform-independent drive syntax, including paths authored on another OS.
    if (relative.find(':') != std::string::npos)
        throw std::runtime_error("Invalid virtual path: " + virtualPath);
    if (mount == "engine") {
        const ResourceLocator locator;
        if (relative.rfind("assets_public/", 0) == 0)
            return inside(locator.publicAsset(""), relative.substr(14));
        if (relative.rfind("shaders/", 0) == 0)
            return inside(locator.shaderDirectory(), relative.substr(8));
        throw std::runtime_error("Unknown engine resource namespace: " + virtualPath);
    }
    const auto found = mounts.find(mount);
    if (found == mounts.end()) throw std::runtime_error("Unknown project mount: " + mount);
    return inside(found->second, relative);
}
}  // namespace azurerender
