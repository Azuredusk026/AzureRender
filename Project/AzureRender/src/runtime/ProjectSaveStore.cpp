#include "runtime/ProjectSaveStore.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>

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
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return nullptr;
    const auto size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxSaveBytes)) return nullptr;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!input.read(bytes.data(), size)) return nullptr;
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
    : directory_(fs::weakly_canonical(fs::absolute(root)) / encodeProjectId(projectId)) {
}

fs::path ProjectSaveStore::path(const std::string& slot) const {
    if (fs::weakly_canonical(directory_) != directory_)
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
    const auto document = readDocument(path(slot));
    if (document.is_null() || document.at("schemaVersion") != SchemaVersion || !document.contains("payload"))
        return nullptr;
    return document.at("payload");
}

void ProjectSaveStore::write(const std::string& slot, const Json& payload) const {
    validateSlot(slot);
    const auto data = Json{{"schemaVersion", SchemaVersion}, {"payload", payload}}.dump();
    if (data.size() > MaxSaveBytes) throw std::invalid_argument("Save slot exceeds one MiB");
    if (directory_.empty()) {
        const std::lock_guard lock(sessionMutex_);
        session_[slot] = payload;
        return;
    }
    const auto target = path(slot);
    const auto existing = readDocument(target);
    if (!existing.is_null() && existing.at("schemaVersion").get<std::uint64_t>() > SchemaVersion)
        throw std::invalid_argument("Newer save version must be preserved");

    fs::create_directories(directory_);
    static std::atomic<std::uint64_t> sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path staging;
    do {
        staging = directory_ / (".candidate-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
    } while (!fs::create_directory(staging));
    const auto candidate = staging / "save.json";
    try {
        {
            std::ofstream output(candidate, std::ios::binary);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.write(data.data(), static_cast<std::streamsize>(data.size()));
            output.close();
        }
        fs::rename(candidate, target);
    } catch (...) {
        std::error_code ignored;
        fs::remove(candidate, ignored);
        fs::remove(staging, ignored);
        throw;
    }
    std::error_code ignored;
    fs::remove(staging, ignored);
}

}
