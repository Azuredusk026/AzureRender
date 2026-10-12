#pragma once
#include <filesystem>
#include <future>
#include <string>

namespace azurerender {
struct GameBuildResult { bool passed = false; std::string message; double milliseconds = 0; };
class GameBuildJob final {
public:
    GameBuildJob(const std::filesystem::path& project, const std::filesystem::path& install,
                 const std::filesystem::path& output, bool replace);
    bool ready() const;
    std::string progress() const;
    GameBuildResult finish();
private:
    std::filesystem::path log_;
    std::future<GameBuildResult> work_;
};
}
