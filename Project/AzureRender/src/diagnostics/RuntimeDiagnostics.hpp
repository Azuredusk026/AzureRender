#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace azurerender {

enum class DiagnosticLevel { Info, Warning, Error };
enum class DiagnosticCode : std::uint32_t {
    None = 0,
    InvalidArguments = 2,
    Asset = 3,
    VulkanInitialization = 4,
    Runtime = 5,
};

class RuntimeDiagnostics final {
public:
    static RuntimeDiagnostics& instance() noexcept;

    void configure(const std::filesystem::path& logPath);
    void log(
        DiagnosticLevel level,
        std::string subsystem,
        DiagnosticCode code,
        std::string message);
    void info(std::string subsystem, std::string message) {
        log(DiagnosticLevel::Info, std::move(subsystem), DiagnosticCode::None,
            std::move(message));
    }
    void error(
        std::string subsystem,
        DiagnosticCode code,
        std::string message) {
        log(DiagnosticLevel::Error, std::move(subsystem), code,
            std::move(message));
    }
    [[nodiscard]] const std::vector<std::string>& messages() const noexcept {
        return messages_;
    }
    [[nodiscard]] bool fileWritable() const noexcept { return fileWritable_; }

private:
    std::filesystem::path logPath_;
    std::vector<std::string> messages_;
    bool fileWritable_ = false;
};

[[nodiscard]] int diagnosticExitCode(const std::string& message) noexcept;
[[nodiscard]] const char* diagnosticLevelName(DiagnosticLevel level) noexcept;

}  // namespace azurerender
