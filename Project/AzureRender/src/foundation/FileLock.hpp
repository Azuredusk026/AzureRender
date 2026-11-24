#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>

namespace azurerender {
enum class FileLockMode { Exclusive, Shared };
// The caller owns the parent directory. The lock file remains after release.
class FileLock final {
public:
    explicit FileLock(const std::filesystem::path& file,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
        std::function<bool()> cancelled = {});
    FileLock(const std::filesystem::path& file, FileLockMode mode,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
        std::function<bool()> cancelled = {});
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
