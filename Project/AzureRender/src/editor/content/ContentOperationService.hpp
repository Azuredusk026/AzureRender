#pragma once
#include "runtime/AssetDatabase.hpp"
#include <memory>
namespace azurerender {
// Owns file journals independently of scene history. All paths are project mounted.
class ContentOperationService final {
public:
    explicit ContentOperationService(Project project);
    const AssetDatabase& database() const {return database_;}
    nlohmann::json plan(const nlohmann::json& request);
    nlohmann::json apply(const std::string& planId,std::function<void()> check={});
    void discard(const std::string& planId) {plans_.erase(planId);}
    void undo();
    void redo();
    void recover();
    static void recoverProject(const std::filesystem::path& projectFile);
    nlohmann::json dependencies(const std::string& asset,const std::string& direction) const;
    nlohmann::json textureSettings(const std::vector<std::string>& assets) const;
    nlohmann::json inspect(const std::string& asset) const;
private:
    Project project_;
    AssetDatabase database_;
    std::filesystem::path journalRoot_;
    struct Change {std::filesystem::path path;std::optional<std::string> before,after;};
    struct Plan {nlohmann::json description,version;std::vector<Change> changes;std::optional<std::filesystem::path> directory;};
    std::map<std::string,Plan> plans_;
    std::vector<std::string> history_;
    std::vector<std::string> redo_;
    nlohmann::json version();
    std::filesystem::path writable(const std::string& reference) const;
    void restore(const std::filesystem::path& journal,bool before);
    static void restoreJournal(const std::filesystem::path& folder,const std::filesystem::path& root,const std::string& projectId,bool before);
};
}
