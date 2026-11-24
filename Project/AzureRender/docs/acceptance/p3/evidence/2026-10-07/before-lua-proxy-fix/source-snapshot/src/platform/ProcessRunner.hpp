#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {
struct ProcessRequest {
    std::filesystem::path executable, workingDirectory;
    std::vector<std::string> arguments;
    std::uint32_t timeoutMs = 60000;
    std::size_t outputBytes = 65536;
};
struct ProcessResult {
    bool passed = false, cancelled = false, timedOut = false;
    std::uint32_t exitCode = 1;
    std::string output, diagnostic;
};
// Runs a directly selected executable; owns its handles and entire process job.
ProcessResult runProcess(const ProcessRequest&, const std::atomic<bool>& cancelled);
}
