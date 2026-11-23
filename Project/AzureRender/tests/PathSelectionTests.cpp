#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "platform/PathSelection.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct DialogSearch { DWORD process; std::wstring title; HWND window = nullptr; };
BOOL CALLBACK findDialog(HWND window, LPARAM argument) {
    auto& search = *reinterpret_cast<DialogSearch*>(argument);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    wchar_t title[256]{};
    GetWindowTextW(window, title, 256);
    if (process == search.process && search.title == title) {
        search.window = window;
        return FALSE;
    }
    return TRUE;
}
struct FieldSearch { HWND field = nullptr; LONG top = LONG_MIN; };
BOOL CALLBACK findFilename(HWND window, LPARAM argument) {
    auto& search = *reinterpret_cast<FieldSearch*>(argument);
    wchar_t name[64]{};
    GetClassNameW(window, name, 64);
    RECT bounds{};
    GetWindowRect(window, &bounds);
    if (std::wstring(name) == L"Edit" && IsWindowEnabled(window)
        && bounds.right > bounds.left && bounds.bottom > bounds.top && bounds.top > search.top) {
        search = {window, bounds.top};
    }
    return TRUE;
}
azurerender::PathSelectionResult select(azurerender::PathSelectionRequest request,
                                       const std::wstring& filenames, bool cancel = false) {
    std::thread input([&] {
        try {
            DialogSearch search{GetCurrentProcessId(), std::filesystem::u8path(request.title).wstring()};
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
            while (!search.window && std::chrono::steady_clock::now() < deadline) {
                EnumWindows(findDialog, reinterpret_cast<LPARAM>(&search));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            require(search.window != nullptr, "Native selection dialog did not open");
            if (!cancel) {
                FieldSearch field;
                while (!field.field && std::chrono::steady_clock::now() < deadline) {
                    EnumChildWindows(search.window, findFilename, reinterpret_cast<LPARAM>(&field));
                    if (!field.field) std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                require(field.field != nullptr, "Native filename input was not found");
                std::cerr << "Filename control: id=" << GetDlgCtrlID(field.field) << '\n';
                DWORD_PTR result = 0;
                require(SendMessageTimeoutW(search.window, WM_NEXTDLGCTL,
                    reinterpret_cast<WPARAM>(field.field), TRUE, SMTO_ABORTIFHUNG, 2000, &result) != 0,
                    "Native filename input could not receive focus");
                require(SendMessageTimeoutW(field.field, WM_SETTEXT, 0,
                    reinterpret_cast<LPARAM>(filenames.c_str()), SMTO_ABORTIFHUNG, 2000, &result) != 0,
                    "Native filename input did not respond");
                wchar_t entered[1024]{};
                GetWindowTextW(field.field, entered, 1024);
                std::cerr << "Filename value: " << std::filesystem::path(entered).u8string() << '\n';
            }
            const auto button = GetDlgItem(search.window, cancel ? IDCANCEL : IDOK);
            DWORD_PTR clicked = 0;
            require(button != nullptr && SendMessageTimeoutW(button, BM_CLICK, 0, 0,
                SMTO_ABORTIFHUNG, 2000, &clicked) != 0,
                    "Native selection confirmation could not be delivered");
            const auto closeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (IsWindow(search.window) && std::chrono::steady_clock::now() < closeDeadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (IsWindow(search.window)) {
                std::cerr << "Selection dialog remained open after confirmation\n";
                PostMessageW(search.window, WM_CLOSE, 0, 0);
            }
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            std::exit(2);
        }
    });
    auto result = azurerender::choosePath(request);
    input.join();
    std::cout << request.title << ": selected=" << result.paths.size()
              << " cancelled=" << result.cancelled << " diagnostic=" << result.diagnostic << '\n';
    return result;
}
}

int main(int argc, char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--external-folder") {
            azurerender::PathSelectionRequest external;
            external.title="Azure external folder";external.directory=true;external.multiple=true;
            const auto selected=azurerender::choosePath(external);
            std::cout<<nlohmann::json{{"path",selected.path.u8string()},{"cancelled",selected.cancelled},{"diagnostic",selected.diagnostic}}.dump()<<'\n';
            return selected.cancelled||!selected.diagnostic.empty()?1:0;
        }
        require(argc == 2, "Expected a managed fixture directory");
        const auto root = std::filesystem::absolute(argv[1]) / std::filesystem::u8path(u8"中文 选择");
        std::filesystem::create_directories(root);
        const auto first = root / std::filesystem::u8path(u8"第一段 空格.wav");
        const auto second = root / "second image.png";
        std::ofstream(first) << "original audio fixture";
        std::ofstream(second) << "original image fixture";
        azurerender::PathSelectionRequest request;
        request.title = "Azure single path contract";
        request.initialDirectory = root;
        request.extensions = {".wav", ".png"};
        const auto one = select(request, L"\"第一段 空格.wav\"");
        require(!one.cancelled && one.diagnostic.empty(), "Single selection failed");
        require(one.paths.size() == 1 && one.path == first && one.paths.front() == first,
                "Single selection must preserve the legacy path and complete result");
        request.title = "Azure multiple path contract";
        request.multiple = true;
        const auto many = select(request, L"\"第一段 空格.wav\" \"second image.png\"");
        require(!many.cancelled && many.diagnostic.empty(), "Multiple selection failed");
        require(many.paths.size() == 2 && many.path == many.paths.front(), "All selected files must be returned");
        require(std::find(many.paths.begin(), many.paths.end(), first) != many.paths.end()
            && std::find(many.paths.begin(), many.paths.end(), second) != many.paths.end(),
            "Multiple selection must retain Unicode, spaces and both media kinds");
        const auto child = root / "Child folder";
        std::filesystem::create_directories(child);
        azurerender::PathSelectionRequest folderRequest{"Azure folder path contract", true, root, {}};
        const auto folder = select(folderRequest, L"\"Child folder\"");
        require(!folder.cancelled && folder.diagnostic.empty() && folder.path == child
            && folder.paths.size() == 1 && folder.paths.front() == child,
            "Legacy folder selection must retain its path and complete result");
        folderRequest.title = "Azure multiple folder path contract";
        folderRequest.multiple = true;
        folderRequest.initialDirectory.clear();
        const auto distinct=root/"Distinct empty folder";std::filesystem::create_directories(distinct);
        const auto multipleFolders = select(folderRequest, distinct.wstring());
        require(!multipleFolders.cancelled && multipleFolders.diagnostic.empty() && multipleFolders.paths.size() == 1
            && multipleFolders.path == distinct, "Multiple folder selection must honor an explicit directory path");
        request.title = "Azure cancelled path contract";
        const auto cancelled = select(request, L"", true);
        require(cancelled.cancelled && cancelled.diagnostic.empty() && cancelled.path.empty() && cancelled.paths.empty(),
                "Cancellation must return no selected paths");
        std::cout << "Native single, multiple, Unicode, spaces and cancellation contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
