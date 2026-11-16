#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace azurerender {

class ProjectSaveStore final {
public:
    ProjectSaveStore() = default;
    ProjectSaveStore(const std::filesystem::path& root, const std::string& projectId);
    [[nodiscard]] nlohmann::json read(const std::string& slot) const;
    void write(const std::string& slot, const nlohmann::json& payload) const;

private:
    [[nodiscard]] std::filesystem::path path(const std::string& slot) const;
    std::filesystem::path directory_;
    mutable std::mutex sessionMutex_;
    mutable std::map<std::string, nlohmann::json> session_;
};

}
