#pragma once
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace azurerender {
struct PathSelectionRequest {
    std::string title;
    bool directory=false;
    std::filesystem::path initialDirectory;
    std::vector<std::string> extensions;
    void* owner=nullptr;
};
struct PathSelectionResult {std::filesystem::path path;bool cancelled=false;std::string diagnostic;};
PathSelectionResult choosePath(const PathSelectionRequest&);
class PathHistory {
public:
    void remember(const std::string& purpose,const std::filesystem::path& path,bool directory);
    std::vector<std::string> directories(const std::string& purpose) const;
    void load(const std::filesystem::path& file);
    void save(const std::filesystem::path& file) const;
private:
    std::map<std::string,std::vector<std::string>> directories_;
};
}
