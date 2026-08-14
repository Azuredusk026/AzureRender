#include "diagnostics/RuntimeDiagnostics.hpp"

#include <cassert>
#include <filesystem>

int main() {
    using namespace azurerender;
    const auto path = std::filesystem::temp_directory_path()
        / "azurerender-diagnostics-test.jsonl";
    auto& diagnostics = RuntimeDiagnostics::instance();
    diagnostics.configure(path);
    diagnostics.info("test", "hello");
    diagnostics.error("test", DiagnosticCode::Asset, "missing asset");
    assert(!diagnostics.messages().empty());
    assert(diagnosticExitCode("Usage: AzureRender") == 2);
    assert(diagnosticExitCode("Unable to open scene") == 3);
    assert(diagnosticExitCode("vkCreateInstance failed") == 4);
    assert(diagnosticExitCode("unexpected editor failure") == 5);
    std::filesystem::remove(path);
    return 0;
}
