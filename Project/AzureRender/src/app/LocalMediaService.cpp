#include "app/LocalMediaService.hpp"
#include "platform/PathCollection.hpp"
#include <stdexcept>

namespace azurerender {
namespace {
AssetDomain domain(const std::string& kind) {
    if (kind == "music") return AssetDomain::Audio;
    if (kind == "images") return AssetDomain::Image;
    throw std::invalid_argument("Media kind must be music or images");
}
std::vector<std::string> extensions(const std::string& kind) {
    return domain(kind) == AssetDomain::Audio ? std::vector<std::string>{".wav", ".mp3", ".flac", ".ogg", ".m4a"}
        : std::vector<std::string>{".png", ".jpg", ".jpeg"};
}
std::string slot(const std::string& kind) { static_cast<void>(domain(kind)); return "local-media-" + kind; }
}
LocalMediaService::LocalMediaService(std::shared_ptr<ProjectSaveStore> store, Selector selector)
    : store_(std::move(store)), selector_(std::move(selector)), progress_(std::make_shared<Progress>()) {
    if (!store_ || !selector_) throw std::invalid_argument("Media service requires storage and a selector");
    static_cast<void>(store_->contentDirectory("local-media-music"));
}
LocalMediaService::~LocalMediaService() { cancel(); if (work_.valid()) work_.wait(); }
void LocalMediaService::cancel() { progress_->cancelled = true; }
void LocalMediaService::poll() {
    if (!work_.valid() || work_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try { const auto result = work_.get(); state_ = result.first; diagnostic_ = result.second; }
    catch (const std::exception& error) { state_ = "failed"; diagnostic_ = error.what(); }
}
nlohmann::json LocalMediaService::status() {
    poll();
    return nlohmann::json::array({state_, progress_->completed.load(), progress_->total.load(), progress_->failed.load(), diagnostic_});
}
bool LocalMediaService::select(const std::string& kind, bool directory) {
    poll(); if (work_.valid()) return false;
    PathSelectionRequest request;
    request.title = kind == "music" ? "导入本地音乐" : "导入本地图片";
    request.directory = directory; request.multiple = true; request.extensions = extensions(kind);
    try {
        const auto selected = selector_(request);
        if (!selected.diagnostic.empty()) { state_ = "failed"; diagnostic_ = selected.diagnostic; return false; }
        if (selected.cancelled) { state_ = "cancelled"; diagnostic_.clear(); return false; }
        auto paths = selected.paths;
        if (paths.empty() && !selected.path.empty()) paths.push_back(selected.path);
        return startPaths(kind, std::move(paths), directory);
    } catch (const std::exception& error) { state_ = "failed"; diagnostic_ = error.what(); return false; }
}
bool LocalMediaService::startPaths(const std::string& kind, std::vector<std::filesystem::path> paths, bool recursive) {
    // A completed future remains owned until the caller observes status. This
    // also makes repeated UI starts deterministic within one input callback.
    if (work_.valid()) return false;
    const auto selectedDomain = domain(kind);
    if (paths.empty()) { state_ = "cancelled"; diagnostic_.clear(); return false; }
    if (paths.size() > 4096) throw std::invalid_argument("Media selection exceeds its input budget");
    progress_ = std::make_shared<Progress>(); diagnostic_.clear(); state_ = "running";
    const auto progress = progress_; const auto store = store_; const auto allowed = extensions(kind);
    work_ = std::async(std::launch::async, [progress, store, paths = std::move(paths), recursive, allowed, kind, selectedDomain] {
        auto cancelled = [progress] { return progress->cancelled.load(); };
        std::string diagnostic;
        try {
            MediaLibrary library(*store, slot(kind), selectedDomain);
            // Validate the current manifest before decoding any selected file.
            static_cast<void>(library.entries());
            PathCollectionRequest request;
            request.inputs = paths; request.extensions = allowed; request.recursive = recursive; request.cancelled = cancelled;
            const auto collected = collectPaths(request);
            if (collected.cancelled) return std::make_pair(std::string("cancelled"), diagnostic);
            if (collected.budgetExceeded) throw std::runtime_error("Media selection exceeds its scan budget");
            for (const auto& issue : collected.issues) {
                ++progress->failed;
                if (diagnostic.size() < 2048) diagnostic += issue.path.filename().u8string() + ": " + issue.diagnostic + "\n";
            }
            progress->total = collected.files.size();
            for (const auto& path : collected.files) {
                if (cancelled()) return std::make_pair(std::string("cancelled"), diagnostic);
                try { library.importFile(path, cancelled); }
                catch (const std::exception& error) {
                    if (cancelled()) return std::make_pair(std::string("cancelled"), diagnostic);
                    ++progress->failed;
                    if (diagnostic.size() < 2048) diagnostic += path.filename().u8string() + ": " + error.what() + "\n";
                }
                ++progress->completed;
            }
            return std::make_pair(std::string(cancelled() ? "cancelled" : "completed"), diagnostic);
        } catch (const std::exception& error) {
            return std::make_pair(std::string(cancelled() ? "cancelled" : "failed"), std::string(error.what()));
        }
    });
    return true;
}
nlohmann::json LocalMediaService::entries(const std::string& kind) const {
    auto result = nlohmann::json::array();
    for (const auto& entry : MediaLibrary(*store_, slot(kind), domain(kind)).entries())
        result.push_back(nlohmann::json::array({entry.id, entry.title, "media:/" + kind + "/" + entry.id, entry.available}));
    return result;
}
std::optional<std::filesystem::path> LocalMediaService::resolve(const std::string& reference) const {
    if (reference.rfind("media:/", 0) != 0) return std::nullopt;
    const auto separator = reference.find('/', 7);
    if (separator == std::string::npos) throw std::invalid_argument("Expected a committed media reference");
    const auto kind = reference.substr(7, separator - 7);
    return MediaLibrary(*store_, slot(kind), domain(kind)).file(reference.substr(separator + 1));
}
}
