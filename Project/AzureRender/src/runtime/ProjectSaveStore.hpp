#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace azurerender {

enum class SaveContentDirectory { Published, Staging };

class ProjectSaveStore final {
public:
    ProjectSaveStore() = default;
    ProjectSaveStore(const std::filesystem::path& root, const std::string& projectId);
    [[nodiscard]] nlohmann::json read(const std::string& slot) const;
    void write(const std::string& slot, const nlohmann::json& payload) const;
    [[nodiscard]] bool writeIfUnchanged(const std::string& slot, const nlohmann::json& expected,
                                         const nlohmann::json& payload) const;
    [[nodiscard]] std::filesystem::path contentDirectory(const std::string& name,
        SaveContentDirectory kind = SaveContentDirectory::Published) const;

private:
    [[nodiscard]] std::filesystem::path path(const std::string& slot) const;
    bool commit(const std::string& slot, const nlohmann::json& payload,
                const nlohmann::json* expected) const;
    std::filesystem::path directory_;
    mutable std::mutex sessionMutex_;
    mutable std::map<std::string, nlohmann::json> session_;
};

}
