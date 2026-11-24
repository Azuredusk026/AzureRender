#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace azurerender {
struct PathCollectionRequest {
    std::vector<std::filesystem::path> inputs;
    std::vector<std::string> extensions;
    bool recursive = false;
    std::size_t maxFiles = 4096;
    std::size_t maxEntries = 16384;
    std::size_t maxDepth = 32;
    std::function<bool()> cancelled;
};
struct PathCollectionIssue {
    std::filesystem::path path;
    std::string diagnostic;
};
struct PathCollectionResult {
    std::vector<std::filesystem::path> files;
    std::vector<PathCollectionIssue> issues;
    std::size_t visitedEntries = 0;
    bool cancelled = false;
    bool budgetExceeded = false;
};
PathCollectionResult collectPaths(const PathCollectionRequest& request);
}
