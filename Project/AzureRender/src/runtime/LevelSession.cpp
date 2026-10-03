#include "runtime/LevelSession.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
namespace azurerender {
LevelSession::LevelSession(Project project, RuntimeLifecycle& runtime)
    : project_(std::move(project)), runtime_(runtime), assets_(project_) {
    assets_.refresh(); commit(load(project_.startupScene), project_.startupScene);
    nextPoll_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
}
Level LevelSession::load(const std::string& reference) const {
    const auto path = assets_.resolveReference(reference);
    if (path.extension() == ".azurelevel") return Level::load(path, assets_);
    Project legacy = project_; legacy.startupScene = reference;
    Level level; level.scene = legacy.loadStartupScene(); return level;
}
void LevelSession::commit(Level candidate, const std::string& reference) {
    runtime_.replaceScene(candidate.scene, [&](auto& runtime) {
        for (const auto& node : candidate.components) installComponents(runtime.world(), runtime.entity(node.first), node.second);
        if (prepare_) prepare_(candidate);
    });
    level_ = std::move(candidate); reference_ = reference; ++revision_;
}
bool LevelSession::poll() {
    try {
        bool refresh = std::chrono::steady_clock::now() >= nextPoll_;
        std::vector<std::string> changed;
        if (refresh) { nextPoll_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500); changed = assets_.refresh(); }
        if (pending_) {
            const auto reference = std::move(*pending_); pending_.reset();
            auto candidate = load(reference); commit(std::move(candidate), reference); error_.clear(); return true;
        }
        if (refresh && !changed.empty()) {
            const auto id = reference_.find(":/") == std::string::npos ? reference_ : assets_.idForPath(reference_);
            if (std::find(changed.begin(), changed.end(), id) != changed.end()) {
                auto candidate = load(reference_);
                if (!candidate.reloadKey().empty() && candidate.reloadKey() == level_.reloadKey()) { error_.clear(); return false; }
                commit(std::move(candidate), reference_); error_.clear(); return true;
            }
        }
    } catch (const std::exception& error) {
        pending_.reset();
        if (error_ != error.what()) RuntimeDiagnostics::instance().warning("runtime", error.what());
        error_ = error.what();
    }
    return false;
}
} // namespace azurerender
