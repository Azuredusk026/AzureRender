#include "RuntimeDiagnostics.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <utility>

namespace azurerender {

namespace {

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(character == '\n' ? ' ' : character);
    }
    return escaped;
}

}  // namespace

RuntimeDiagnostics& RuntimeDiagnostics::instance() noexcept {
    static RuntimeDiagnostics diagnostics;
    return diagnostics;
}

void RuntimeDiagnostics::configure(const std::filesystem::path& logPath) {
    logPath_ = logPath;
    fileWritable_ = false;
    if (!logPath_.empty()) {
        std::error_code error;
        if (!logPath_.parent_path().empty()) {
            std::filesystem::create_directories(logPath_.parent_path(), error);
        }
        std::ofstream output(logPath_, std::ios::app);
        fileWritable_ = output.good();
    }
}

const char* diagnosticLevelName(const DiagnosticLevel level) noexcept {
    switch (level) {
    case DiagnosticLevel::Info: return "info";
    case DiagnosticLevel::Warning: return "warning";
    case DiagnosticLevel::Error: return "error";
    }
    return "unknown";
}

void RuntimeDiagnostics::log(
    const DiagnosticLevel level,
    std::string subsystem,
    const DiagnosticCode code,
    std::string message) {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(now).count();
    const std::string line = "[" + std::to_string(milliseconds) + "]["
        + diagnosticLevelName(level) + "][" + subsystem + "][code="
        + std::to_string(static_cast<std::uint32_t>(code)) + "] " + message;
    constexpr std::size_t kCapacity = 512;
    if (messages_.size() == kCapacity) {
        messages_.erase(messages_.begin());
    }
    messages_.push_back(line);
    std::cerr << line << '\n';
    if (fileWritable_) {
        std::ofstream output(logPath_, std::ios::app);
        if (output) {
            output << "{\"timestamp_ms\":" << milliseconds
                   << ",\"level\":\"" << diagnosticLevelName(level)
                   << "\",\"subsystem\":\"" << jsonEscape(subsystem)
                   << "\",\"code\":"
                   << static_cast<std::uint32_t>(code)
                   << ",\"message\":\"" << jsonEscape(message)
                   << "\"}\n";
        } else {
            fileWritable_ = false;
        }
    }
}

int diagnosticExitCode(const std::string& message) noexcept {
    if (message.rfind("Usage:", 0) == 0
        || message.find("--") != std::string::npos) {
        return static_cast<int>(DiagnosticCode::InvalidArguments);
    }
    if (message.find("asset") != std::string::npos
        || message.find("scene") != std::string::npos
        || message.find("glTF") != std::string::npos) {
        return static_cast<int>(DiagnosticCode::Asset);
    }
    if (message.find("Vulkan") != std::string::npos
        || message.find("vk") != std::string::npos
        || message.find("GPU") != std::string::npos
        || message.find("GLFW") != std::string::npos) {
        return static_cast<int>(DiagnosticCode::VulkanInitialization);
    }
    return static_cast<int>(DiagnosticCode::Runtime);
}

}  // namespace azurerender
