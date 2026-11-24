#include "platform/PathCollection.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#endif

namespace azurerender {
namespace {
namespace fs = std::filesystem;
std::string lowercase(std::string value) {
    for (auto& letter : value) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return value;
}
bool directoryLink(const fs::path& path) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code error;
    return fs::is_symlink(fs::symlink_status(path, error));
#endif
}
std::string identity(const fs::path& path, std::error_code& error) {
#ifdef _WIN32
    const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { error = {static_cast<int>(GetLastError()), std::system_category()}; return {}; }
    BY_HANDLE_FILE_INFORMATION info{};
    const auto read = GetFileInformationByHandle(handle, &info);
    if (!read) error = {static_cast<int>(GetLastError()), std::system_category()};
    CloseHandle(handle);
    if (!read) return {};
    return std::to_string(info.dwVolumeSerialNumber) + ":" + std::to_string(info.nFileIndexHigh) + ":" + std::to_string(info.nFileIndexLow);
#else
    struct stat info{};
    if (::stat(path.c_str(), &info) != 0) { error = {errno, std::generic_category()}; return {}; }
    return std::to_string(static_cast<std::uintmax_t>(info.st_dev)) + ":" + std::to_string(static_cast<std::uintmax_t>(info.st_ino));
#endif
}
class Collection {
public:
    explicit Collection(const PathCollectionRequest& request) : request_(request) {
        if (request.maxFiles == 0 || request.maxFiles > 100000 || request.maxEntries == 0
            || request.maxEntries > 1000000 || request.maxDepth == 0 || request.maxDepth > 128)
            throw std::invalid_argument("Invalid path collection budget");
        for (const auto& extension : request.extensions) {
            if (extension.size() < 2 || extension.front() != '.' || extension.find_first_of("/\\*?") != std::string::npos)
                throw std::invalid_argument("Path collection extensions require a leading dot and no wildcard");
            extensions_.insert(lowercase(extension));
        }
    }
    PathCollectionResult run() {
        if (request_.inputs.size() > request_.maxEntries) {
            budget({}, "Selected inputs exceed the path collection entry budget");
            return result_;
        }
        auto inputs = request_.inputs;
        std::sort(inputs.begin(), inputs.end());
        for (const auto& input : inputs) visit(input, 0, false);
        if (check()) {
            for (const auto& item : files_) result_.files.push_back(item.second);
            std::sort(result_.files.begin(), result_.files.end());
        }
        return result_;
    }
private:
    const PathCollectionRequest& request_;
    PathCollectionResult result_;
    std::set<std::string> extensions_, directories_;
    std::map<std::string, fs::path> files_;
    bool active() const { return !result_.cancelled && !result_.budgetExceeded; }
    void budget(const fs::path& path, const char* message) {
        result_.budgetExceeded = true;
        result_.issues.push_back({path, message});
    }
    bool check() {
        if (!active()) return false;
        if (request_.cancelled && request_.cancelled()) { result_.cancelled = true; return false; }
        return true;
    }
    bool touch(const fs::path& path) {
        if (!check()) return false;
        if (result_.visitedEntries >= request_.maxEntries) {
            budget(path, "Path collection entry budget exceeded");
            return false;
        }
        ++result_.visitedEntries;
        return true;
    }
    void visit(const fs::path& path, std::size_t depth, bool counted) {
        if (counted ? !check() : !touch(path)) return;
        std::error_code error;
        const auto status = fs::status(path, error);
        if (error) { result_.issues.push_back({path, error.message()}); return; }
        if (!fs::is_directory(status) && !fs::is_regular_file(status)) {
            result_.issues.push_back({path, "Selected path is not a regular file or directory"});
            return;
        }
        if (fs::is_directory(status) && depth > 0) {
            if (!request_.recursive || directoryLink(path)) return;
            if (depth > request_.maxDepth) { budget(path, "Path collection directory depth budget exceeded"); return; }
        }
        if (fs::is_regular_file(status) && !extensions_.empty()
            && extensions_.count(lowercase(path.extension().u8string())) == 0) return;
        const auto canonical = fs::canonical(path, error);
        if (error) { result_.issues.push_back({path, error.message()}); return; }
        const auto key = identity(canonical, error);
        if (error) { result_.issues.push_back({path, error.message()}); return; }
        if (fs::is_regular_file(status)) {
            if (files_.count(key)) return;
            if (files_.size() >= request_.maxFiles) { budget(path, "Path collection file budget exceeded"); return; }
            files_.emplace(key, canonical);
            return;
        }
        if (!directories_.insert(key).second) return;
        fs::directory_iterator iterator(canonical, error), end;
        if (error) { result_.issues.push_back({path, error.message()}); return; }
        std::vector<fs::path> entries;
        while (iterator != end) {
            if (!touch(iterator->path())) return;
            entries.push_back(iterator->path());
            iterator.increment(error);
            if (error) { result_.issues.push_back({path, error.message()}); break; }
        }
        std::sort(entries.begin(), entries.end());
        for (const auto& entry : entries) visit(entry, depth + 1, true);
    }
};
}
PathCollectionResult collectPaths(const PathCollectionRequest& request) { return Collection(request).run(); }
}
