#pragma once

#include "AzureRenderOptions.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace azurerender {

enum class CommandLineErrorCode : std::uint8_t {
    UnknownOption = 1,
    MissingValue = 2,
    InvalidValue = 3,
    InvalidCombination = 4,
};

class CommandLineError final : public std::invalid_argument {
public:
    CommandLineError(
        CommandLineErrorCode code,
        std::string option,
        std::string message);

    [[nodiscard]] CommandLineErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& option() const noexcept { return option_; }

private:
    CommandLineErrorCode code_;
    std::string option_;
};

struct ParsedCommandLine {
    AzureRenderOptions options;
    std::string scenePath;
    std::string createScenePath;
    std::string editorScenePath;
    bool showVersion = false;
    bool checkResources = false;
};

[[nodiscard]] ParsedCommandLine parseCommandLine(
    const std::vector<std::string>& arguments);
[[nodiscard]] const char* commandLineUsage() noexcept;

}  // namespace azurerender
