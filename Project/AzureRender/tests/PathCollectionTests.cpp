#include "platform/PathCollection.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void file(const fs::path& path) { fs::create_directories(path.parent_path()); std::ofstream(path) << "original fixture"; }
void directoryLink(const fs::path& link, const fs::path& target) {
#ifdef _WIN32
    require(GetFileAttributesW(link.c_str()) == INVALID_FILE_ATTRIBUTES, "Fixture link name already exists");
    fs::create_directory(link);
    const std::wstring substitute = L"\\??\\" + target.native();
    const auto print = target.native();
    const auto bytes = 16 + (substitute.size() + print.size() + 2) * sizeof(wchar_t);
    std::vector<unsigned char> buffer(bytes, 0);
    auto* tag = reinterpret_cast<DWORD*>(buffer.data());
    auto* lengths = reinterpret_cast<WORD*>(buffer.data() + 4);
    auto* offsets = reinterpret_cast<WORD*>(buffer.data() + 8);
    *tag = IO_REPARSE_TAG_MOUNT_POINT;
    lengths[0] = static_cast<WORD>(bytes - 8);
    offsets[0] = 0;
    offsets[1] = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
    offsets[2] = static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t));
    offsets[3] = static_cast<WORD>(print.size() * sizeof(wchar_t));
    std::memcpy(buffer.data() + 16, substitute.c_str(), (substitute.size() + 1) * sizeof(wchar_t));
    std::memcpy(buffer.data() + 16 + offsets[2], print.c_str(), (print.size() + 1) * sizeof(wchar_t));
    const auto handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    require(handle != INVALID_HANDLE_VALUE, "Cannot open the owned junction fixture");
    DWORD returned = 0;
    const auto created = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
        static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr);
    CloseHandle(handle);
    require(created != FALSE, "Cannot create the owned cyclic junction fixture");
#else
    require(!fs::exists(link) && !fs::is_symlink(link), "Fixture link name already exists");
    fs::create_directory_symlink(target, link);
#endif
}
struct OwnedDirectoryLink {
    fs::path path, target;
    bool created = false;
    ~OwnedDirectoryLink() {
        if (!created) return;
        std::error_code error;
        const auto relative = path.lexically_relative(target);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
            || !fs::equivalent(path, target, error) || error) std::terminate();
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0
            || !RemoveDirectoryW(path.c_str())) std::terminate();
#else
        if (!fs::is_symlink(path) || !fs::remove(path, error) || error) std::terminate();
#endif
    }
};
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected a managed fixture directory");
        const auto run = "case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto root = fs::absolute(argv[1]) / run / fs::u8path(u8"媒体 目录");
        root.make_preferred();
        const auto first = root / "first.WAV";
        const auto second = root / "nested/second.Mp3";
        file(first); file(second); file(root / "skip.txt");
        file(root / "nested/third.FLAC"); file(root / "nested/fourth.ogg"); file(root / "nested/last.m4a");
        file(root / fs::u8path(u8"图片.PNG")); file(root / "nested/photo.JpEg");
        file(root / "nested/another.jpg");
        OwnedDirectoryLink loop{root / "nested/loop", root};
        directoryLink(loop.path, root);
        loop.created = true;
        azurerender::PathCollectionRequest request;
        request.inputs = {root};
        request.extensions = {".wav", ".mp3", ".flac", ".ogg", ".m4a"};
        request.recursive = true;
        const auto audio = azurerender::collectPaths(request);
        require(audio.files.size() == 5 && audio.issues.empty() && !audio.cancelled && !audio.budgetExceeded,
                "Recursive audio discovery must filter extensions and finish through a cyclic link");
        require(std::is_sorted(audio.files.begin(), audio.files.end()), "Collection order must be stable");
        request.recursive = false;
        const auto flat = azurerender::collectPaths(request);
        require(flat.files.size() == 1 && fs::equivalent(flat.files.front(), first),
                "Flat discovery must retain only the selected directory's files");
        request.recursive = true;
        request.extensions = {".png", ".jpg", ".jpeg"};
        const auto images = azurerender::collectPaths(request);
        require(images.files.size() == 3 && images.issues.empty(), "Image discovery must share the collection contract");
        const auto alias = root / "alias.wav";
        if (!fs::exists(alias)) fs::create_hard_link(first, alias);
        request.inputs = {first, alias, root / "nested/../first.WAV", root / "nested/loop/first.WAV"};
        request.extensions = {".WAV"};
        const auto unique = azurerender::collectPaths(request);
        require(unique.files.size() == 1 && fs::equivalent(unique.files.front(), first),
                "Duplicate paths, hard links and explicit directory aliases identify one file");
        std::reverse(request.inputs.begin(), request.inputs.end());
        require(azurerender::collectPaths(request).files == unique.files, "Input order must not change collection output");
        request.inputs = {first, root / "missing.wav"};
        const auto missing = azurerender::collectPaths(request);
        require(missing.files.size() == 1 && missing.issues.size() == 1 && !missing.issues.front().diagnostic.empty(),
                "Missing input must be diagnosed beside usable selections");
        request.inputs = {root};
        int checks = 0;
        request.cancelled = [&] { return ++checks >= 3; };
        const auto cancelled = azurerender::collectPaths(request);
        require(cancelled.cancelled && cancelled.files.empty(), "Cancellation must not expose a partial collection");
        request.cancelled = {};
        request.extensions = {};
        request.maxFiles = 1;
        const auto files = azurerender::collectPaths(request);
        require(files.budgetExceeded && files.files.empty(), "File count budget must reject partial publication");
        request.maxFiles = 4096;
        request.maxEntries = 1;
        const auto entries = azurerender::collectPaths(request);
        require(entries.budgetExceeded && entries.files.empty(), "Traversal budget must bound unsupported entries too");
        request.maxEntries = 16384;
        request.maxDepth = 1;
        file(root / "nested/deeper/too-deep.wav");
        const auto depth = azurerender::collectPaths(request);
        require(depth.budgetExceeded && depth.files.empty(), "Directory depth budget must reject partial publication");
        request.maxDepth = 32;
        request.maxFiles = 0;
        bool rejected = false;
        try { static_cast<void>(azurerender::collectPaths(request)); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid collection budgets must be rejected before traversal");
        request.maxFiles = 4096;
        request.inputs = {};
        request.cancelled = [] { return true; };
        const auto emptyCancelled = azurerender::collectPaths(request);
        require(emptyCancelled.cancelled && emptyCancelled.files.empty(),
                "A pre-cancelled empty selection must report cancellation");
        request.cancelled = {};
        request.extensions = {"*.wav"};
        rejected = false;
        try { static_cast<void>(azurerender::collectPaths(request)); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid extension filters must be rejected before traversal");
        std::cout << "Audio, images, recursion, Unicode, links, deduplication, missing paths, cancellation and three budgets passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
