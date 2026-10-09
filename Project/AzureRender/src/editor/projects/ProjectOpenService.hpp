#pragma once
#include "editor/EditorContext.hpp"
#include <functional>
namespace azurerender {
// Owns project preparation and bounded, user-local recent records.
// Activation belongs to EditorSession and happens only at a frame boundary.
class ProjectOpenService {
public:
    explicit ProjectOpenService(std::filesystem::path history);
    std::shared_ptr<EditorContext> create(const std::string& templateId,
        const std::filesystem::path& destination,const std::string& name,
        const std::function<bool()>& cancelled={});
    std::shared_ptr<EditorContext> open(const std::filesystem::path& path) const;
    void remember(const Project& project);
    nlohmann::json recent() const;
    static nlohmann::json templates();
private:
    std::filesystem::path history_;
    nlohmann::json records_=nlohmann::json::array();
};
}
