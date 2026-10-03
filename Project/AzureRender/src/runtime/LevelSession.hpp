#pragma once
#include "runtime/Level.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include <chrono>
#include <optional>
namespace azurerender {
class LevelSession {
public:
    LevelSession(Project project, RuntimeLifecycle& runtime);
    void request(std::string reference) { pending_ = std::move(reference); }
    bool poll();
    void setPrepareHandler(std::function<void(const Level&)> handler) { prepare_ = std::move(handler); }
    AssetDatabase& assets() noexcept { return assets_; }
    const Level& current() const noexcept { return level_; }
    const std::string& lastError() const noexcept { return error_; }
    std::uint64_t revision() const noexcept { return revision_; }
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
};
} // namespace azurerender
