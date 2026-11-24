#include "editor/GameBuildJob.hpp"
#include "resources/ResourceLocator.hpp"
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace azurerender {
namespace {
#ifdef _WIN32
// Windows CRT argument rules, including trailing backslashes before a quote.
std::wstring quote(const std::wstring& value) {
    std::wstring result = L"\""; std::size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c; slashes = 0;
    }
    result.append(slashes * 2, L'\\'); return result + L'"';
}
#endif
}
GameBuildJob::GameBuildJob(const std::filesystem::path& project, const std::filesystem::path& install,
                          const std::filesystem::path& output, bool replace) {
    const auto tool = ResourceLocator().publicAsset("test_model.gltf").parent_path().parent_path() / "tools/build_game.py";
    if (!std::filesystem::is_regular_file(tool)) throw std::runtime_error("Game build tool missing: " + tool.string());
    const auto log = project.parent_path() / ".azure/game-build.log";
    std::filesystem::create_directories(log.parent_path());
    work_ = std::async(std::launch::async, [=] {
        const auto start = std::chrono::steady_clock::now(); GameBuildResult result;
        try {
#ifdef _WIN32
        std::wstring command = L"python " + quote(tool.wstring()) + L" --project " + quote(project.wstring())
            + L" --install " + quote(std::filesystem::absolute(install).wstring())
            + L" --output " + quote(std::filesystem::absolute(output).wstring()) + (replace ? L" --replace" : L"");
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE file = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) { result.message = "Cannot open game build log"; return result; }
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = startup.hStdError = file;
        startup.hStdInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr);
        PROCESS_INFORMATION process{};
        const bool launched = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0;
        const auto error = GetLastError();
        CloseHandle(file); if(startup.hStdInput != INVALID_HANDLE_VALUE) CloseHandle(startup.hStdInput);
        if (launched) {
            const auto waited = WaitForSingleObject(process.hProcess, 120000);
            if (waited != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000); }
            DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
            result.passed = waited == WAIT_OBJECT_0 && code == 0;
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            std::ifstream input(log); result.message.assign(std::istreambuf_iterator<char>(input), {});
            if(waited != WAIT_OBJECT_0) result.message += "\nGame build exceeded 120 seconds";
        } else result.message = "Cannot start Python 3.11+ game builder; Windows error " + std::to_string(error);
#else
        result.message = "Game publishing requires Windows x64";
#endif
        } catch (const std::exception& error) { result.passed = false; result.message = error.what(); }
        result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return result;
    });
}
bool GameBuildJob::ready() const { return work_.valid() && work_.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
GameBuildResult GameBuildJob::finish() { return work_.get(); }
}
