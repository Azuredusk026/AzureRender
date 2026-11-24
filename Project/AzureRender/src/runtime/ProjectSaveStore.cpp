#include "runtime/ProjectSaveStore.hpp"
#include "foundation/NativeFilePath.hpp"
#include "foundation/FileLock.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace azurerender {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::size_t MaxSaveBytes = 1024 * 1024;
constexpr unsigned SchemaVersion = 1;

std::string encodeProjectId(const std::string& id) {
    if (id.empty() || id.size() > 100) throw std::invalid_argument("Invalid save project identity");
    constexpr char digits[] = "0123456789abcdef";
    std::string name = "project-";
    name.reserve(name.size() + 2 * id.size());
    for (const auto value : id) {
        const auto byte = static_cast<unsigned char>(value);
        name += digits[byte >> 4U];
        name += digits[byte & 15U];
    }
    return name;
}

Json readDocument(const fs::path& path) {
    if (fs::is_symlink(path)) throw std::invalid_argument("Save file must not be a symbolic link");
#ifdef _WIN32
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return nullptr;
    const auto close = [](void* value) { CloseHandle(static_cast<HANDLE>(value)); };
    std::unique_ptr<void, decltype(close)> owned(file, close);
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > static_cast<LONGLONG>(MaxSaveBytes))
        return nullptr;
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size())
        return nullptr;
    owned.reset();
#else
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return nullptr;
    const auto size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxSaveBytes)) return nullptr;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!input.read(bytes.data(), size)) return nullptr;
#endif
    auto result = Json::parse(bytes, nullptr, false);
    if (!result.is_object() || !result.contains("schemaVersion") ||
        !result.at("schemaVersion").is_number_unsigned())
        return nullptr;
    return result;
}

void validateSlot(const std::string& slot) {
    if (slot.empty() || slot.size() > 64 ||
        !std::all_of(slot.begin(), slot.end(), [](char value) {
            return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
                   value == '-' || value == '_';
        }))
        throw std::invalid_argument("Invalid save slot name");
}

}

ProjectSaveStore::ProjectSaveStore(const fs::path& root, const std::string& projectId)
    : directory_(nativeFilePath(fs::weakly_canonical(nativeFilePath(fs::absolute(root)))) /
                 encodeProjectId(projectId)) {
}

fs::path ProjectSaveStore::path(const std::string& slot) const {
    if (nativeFilePath(fs::weakly_canonical(directory_)) != directory_)
        throw std::invalid_argument("Save project directory resolves outside its assigned path");
    return directory_ / ("save-" + slot + ".json");
}

Json ProjectSaveStore::read(const std::string& slot) const {
    validateSlot(slot);
    if (directory_.empty()) {
        const std::lock_guard lock(sessionMutex_);
        const auto found = session_.find(slot);
        return found == session_.end() ? Json(nullptr) : found->second;
    }
    const auto target = path(slot);
    const auto lockPath = directory_ / (".save-" + slot + ".lock");
    auto lockedDocument = [&] {
        const FileLock lock(lockPath, FileLockMode::Shared);
        return readDocument(target);
    };
    const auto coordinated = fs::exists(lockPath);
    auto document = coordinated ? lockedDocument() : readDocument(target);
    // A legacy slot gains its coordination file before its first current write.
    if (!coordinated && fs::exists(lockPath)) document = lockedDocument();
    if (document.is_null() || document.at("schemaVersion") != SchemaVersion || !document.contains("payload"))
        return nullptr;
    return document.at("payload");
}

void ProjectSaveStore::write(const std::string& slot, const Json& payload) const {
    static_cast<void>(commit(slot, payload, nullptr));
}

bool ProjectSaveStore::writeIfUnchanged(const std::string& slot, const Json& expected, const Json& payload) const {
    return commit(slot, payload, &expected);
}

fs::path ProjectSaveStore::contentDirectory(const std::string& name, SaveContentDirectory kind) const {
    validateSlot(name);
    if (directory_.empty()) throw std::logic_error("Persistent content requires a save root");
    static_cast<void>(path(name));
    const auto child = directory_ / (kind == SaveContentDirectory::Published
        ? "content-" + name : ".content-" + name + "-staging");
    if (nativeFilePath(fs::weakly_canonical(child)) != child)
        throw std::invalid_argument("Content directory resolves outside its assigned path");
    return child;
}

bool ProjectSaveStore::commit(const std::string& slot, const Json& payload, const Json* expected) const {
    validateSlot(slot);
    const auto data = Json{{"schemaVersion", SchemaVersion}, {"payload", payload}}.dump();
    if (data.size() > MaxSaveBytes) throw std::invalid_argument("Save slot exceeds one MiB");
    if (directory_.empty()) {
        const std::lock_guard lock(sessionMutex_);
        const auto found = session_.find(slot);
        const auto current = found == session_.end() ? Json(nullptr) : found->second;
        if (expected && current != *expected) return false;
        session_[slot] = payload;
        return true;
    }
    const auto target = path(slot);
    fs::create_directories(directory_);
    const FileLock lock(directory_ / (".save-" + slot + ".lock"));
    const auto existing = readDocument(target);
    if (!existing.is_null() && existing.at("schemaVersion").get<std::uint64_t>() > SchemaVersion)
        throw std::invalid_argument("Newer save version must be preserved");
    const auto current = existing.is_null() || existing.at("schemaVersion") != SchemaVersion
        || !existing.contains("payload") ? Json(nullptr) : existing.at("payload");
    if (expected && current != *expected) return false;

    static std::atomic<std::uint64_t> sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path staging;
    do {
        staging = directory_ / (".candidate-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
    } while (!fs::create_directory(staging));
    const auto candidate = staging / "save.json";
#ifdef _WIN32
    const auto backup = staging / "previous.json";
#endif
    try {
        {
            std::ofstream output(candidate, std::ios::binary);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.write(data.data(), static_cast<std::streamsize>(data.size()));
            output.close();
        }
#ifdef _WIN32
        if (fs::exists(target)) {
            if (!ReplaceFileW(target.c_str(), candidate.c_str(), backup.c_str(), 0, nullptr, nullptr))
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot replace save snapshot");
        } else if (!MoveFileExW(candidate.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot publish save snapshot");
        }
#else
        fs::rename(candidate, target);
#endif
    } catch (...) {
        std::error_code ignored;
#ifdef _WIN32
        if (fs::exists(backup, ignored) && !fs::exists(target, ignored))
            static_cast<void>(MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH));
#endif
        fs::remove(candidate, ignored);
        fs::remove(staging, ignored);
        throw;
    }
    std::error_code ignored;
#ifdef _WIN32
    fs::remove(backup, ignored);
#endif
    fs::remove(staging, ignored);
    return true;
}

}
