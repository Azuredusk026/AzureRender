#include "diagnostics/RuntimeDiagnostics.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool fileContains(
    const std::filesystem::path& path,
    const std::string& needle) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    using namespace azurerender;
    const auto path = std::filesystem::temp_directory_path()
        / "azurerender-diagnostics-test.jsonl";
    auto& diagnostics = RuntimeDiagnostics::instance();
    diagnostics.configure(path);
    diagnostics.info("test", "hello");
    diagnostics.warning("test", "soft warning");
    diagnostics.error("test", DiagnosticCode::Asset, "missing asset");
    diagnostics.print("test", "user-facing line");
    assert(!diagnostics.messages().empty());
    // print() is recorded in the same in-memory event stream.
    bool sawPrint = false;
    for (const std::string& line : diagnostics.messages()) {
        if (line.find("user-facing line") != std::string::npos) {
            sawPrint = true;
        }
    }
    assert(sawPrint);
    assert(diagnosticExitCode("Usage: AzureRender") == 2);
    assert(diagnosticExitCode("Unable to open scene") == 3);
    assert(diagnosticExitCode("vkCreateInstance failed") == 4);
    assert(diagnosticExitCode("unexpected editor failure") == 5);
    // The JSON file records info, warning, error and print events.
    assert(fileContains(path, "\"level\":\"info\""));
    assert(fileContains(path, "\"level\":\"warning\""));
    assert(fileContains(path, "\"level\":\"error\""));
    assert(fileContains(path, "\"level\":\"print\""));
    std::filesystem::remove(path);
    return 0;
}
