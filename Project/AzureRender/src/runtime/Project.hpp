#pragma once
#include <filesystem>
#include <map>
#include <string>

#include "SceneDocument.hpp"
namespace azurerender {
class Project final {
   public:
    static constexpr unsigned kSchemaVersion = 1;
    std::filesystem::path file;
    std::string id, name, startupScene;
    std::map<std::string, std::filesystem::path> mounts;
    static void create(const std::filesystem::path& directory, const std::string& name);
    static void createGame(const std::filesystem::path& directory, const std::string& name);
    static Project load(const std::filesystem::path& file);
    [[nodiscard]] SceneDocument loadStartupScene() const;
    [[nodiscard]] std::filesystem::path resolve(const std::string& virtualPath) const;
};
}  // namespace azurerender
