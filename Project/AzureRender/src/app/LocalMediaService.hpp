#pragma once
#include "runtime/IMediaLibraryService.hpp"
#include "runtime/MediaLibrary.hpp"
#include "platform/PathSelection.hpp"
#include <atomic>
#include <future>
#include <optional>

namespace azurerender {
class LocalMediaService final : public IMediaLibraryService {
public:
    using Selector = std::function<PathSelectionResult(const PathSelectionRequest&)>;
    explicit LocalMediaService(std::shared_ptr<ProjectSaveStore> store, Selector selector = choosePath);
    ~LocalMediaService() override;
    bool select(const std::string& kind, bool directory) override;
    bool startPaths(const std::string& kind, std::vector<std::filesystem::path> paths, bool recursive);
    nlohmann::json entries(const std::string& kind) const override;
    nlohmann::json status() override;
    void cancel() override;
    [[nodiscard]] std::optional<std::filesystem::path> resolve(const std::string& reference) const;
private:
    struct Progress {
        std::atomic<bool> cancelled{false};
        std::atomic<std::size_t> completed{0}, total{0}, failed{0};
    };
    void poll();
    std::shared_ptr<ProjectSaveStore> store_;
    Selector selector_;
    std::shared_ptr<Progress> progress_;
    std::future<std::pair<std::string, std::string>> work_;
    std::string state_ = "idle", diagnostic_;
};
}
