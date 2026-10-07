#pragma once
#include "runtime/Level.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include <chrono>
#include <optional>
#include <future>
#include <atomic>
namespace azurerender {
class LevelSession {
public:
    LevelSession(Project project, RuntimeLifecycle& runtime);
    ~LevelSession();
    void request(std::string reference);
    void preload(const std::string& reference);
    bool preloaded(const std::string& reference) const {
        const auto found=prepared_.find(reference);if(found==prepared_.end())return false;
        for(const auto& record:assets_.records()){
            const auto cached=found->second.assets.records().find(record.first);
            if(cached==found->second.assets.records().end() || cached->second.fingerprint!=record.second.fingerprint || cached->second.path!=record.second.path)return false;
        }
        return !ready_ || ready_(found->second.level);
    }
    bool loading() const { return pending_.has_value() || work_.valid() || preload_.has_value() || preparing_.has_value(); }
    void cancelPending();
    bool poll();
    void setPrepareHandler(std::function<void(const Level&)> handler) { prepare_ = std::move(handler); }
    void setPreloadHandler(std::function<bool(const Level&)> handler) { upload_=std::move(handler); }
    void setReadinessHandler(std::function<bool(const Level&)> handler) { ready_=std::move(handler); }
    AssetDatabase& assets() noexcept { return assets_; }
    const Project& project() const noexcept { return project_; }
    const Level& current() const noexcept { return level_; }
    const std::string& currentReference() const noexcept { return reference_; }
    const std::string& lastError() const noexcept { return error_; }
    std::uint64_t revision() const noexcept { return revision_; }
    double lastCommitMilliseconds() const noexcept { return commitMilliseconds_; }
    std::size_t cachedCandidates() const noexcept { return prepared_.size(); }
    std::uint64_t requestGeneration() const noexcept { return generation_; }
private:
    Level load(const std::string& reference) const;
    void commit(Level candidate, const std::string& reference);
    std::function<void(const Level&)> prepare_;
    Project project_;
    RuntimeLifecycle& runtime_;
    AssetDatabase assets_;
    Level level_;
    std::string reference_, error_;
    std::optional<std::string> pending_;
    std::chrono::steady_clock::time_point nextPoll_{};
    std::uint64_t revision_ = 0;
    double commitMilliseconds_=0;
    struct Candidate { AssetDatabase assets; Level level; std::string reference; std::uint64_t generation; bool refresh; };
    std::future<Candidate> work_;
    std::map<std::string,Candidate> prepared_;
    std::optional<Candidate> preparing_;
    std::function<bool(const Level&)> upload_;
    std::function<bool(const Level&)> ready_;
    std::optional<std::string> preload_;
    std::shared_ptr<std::atomic<bool>> cancelled_;
    std::uint64_t generation_=0;
    void startWork(const std::string& reference,bool refresh);
    bool accept(Candidate candidate);
};
} // namespace azurerender
