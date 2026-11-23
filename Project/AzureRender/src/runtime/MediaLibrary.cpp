#include "runtime/MediaLibrary.hpp"
#include "foundation/FileLock.hpp"
#include "foundation/NativeFilePath.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <random>
#include <set>
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
void check(const AssetCancellation& cancelled) {
    if (cancelled && cancelled()) throw std::runtime_error("Media import cancelled");
}
bool identifier(const std::string& value) {
    return !value.empty() && value.size() <= 96 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
bool filename(const std::string& value) {
    const auto path = fs::u8path(value);
    return !value.empty() && value != "." && value != ".." && path.filename() == path
        && value.find_first_of("/\\:") == std::string::npos;
}
bool redirected(const fs::path& path) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(nativeFilePath(path).c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
#endif
    return fs::is_symlink(path) || nativeFilePath(fs::weakly_canonical(path)) != nativeFilePath(path);
}
std::string token() {
    static std::atomic<std::uint64_t> sequence{0};
    static const auto entropy = std::random_device{}();
    return "m" + std::to_string(entropy) + "-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
}
Json encode(const std::vector<MediaLibraryEntry>& entries) {
    auto rows = Json::array();
    for (const auto& e : entries) rows.push_back(Json::array({e.id, e.title, e.format, e.blob, e.filename, e.fingerprint, e.bytes}));
    return Json::array({1, rows});
}
std::uint64_t cacheBytes(const fs::path& root, const MediaLibraryLimits& limits, const AssetCancellation& cancelled) {
    if (!fs::exists(root)) return 0;
    if (redirected(root)) throw std::runtime_error("Media cache directory is redirected");
    std::uint64_t bytes = 0;
    std::size_t scanned = 0;
    for (const auto& item : fs::recursive_directory_iterator(root)) {
        check(cancelled);
        if (++scanned > limits.maxScanEntries) throw std::runtime_error("Media cache scan exceeds its entry budget");
        if (redirected(item.path())) throw std::runtime_error("Media cache contains redirected content");
        if (item.is_regular_file()) {
            const auto size = item.file_size();
            if (size > limits.maxCacheBytes - bytes) throw std::runtime_error("Media cache exceeds its byte budget");
            bytes += size;
        } else if (!item.is_directory()) throw std::runtime_error("Media cache contains nonregular content");
    }
    return bytes;
}
bool sameBytes(const fs::path& first, const fs::path& second, const AssetCancellation& cancelled) {
    if (fs::file_size(first) != fs::file_size(second)) return false;
    std::ifstream a(first, std::ios::binary), b(second, std::ios::binary);
    if (!a || !b) throw std::runtime_error("Cannot compare imported media bytes");
    std::array<char, 65536> left{}, right{};
    do {
        check(cancelled); a.read(left.data(), left.size()); b.read(right.data(), right.size());
        if (a.gcount() != b.gcount() || !std::equal(left.begin(), left.begin() + a.gcount(), right.begin())) return false;
    } while (a && b);
    if (!a.eof() || !b.eof()) throw std::runtime_error("Cannot read complete imported media bytes");
    return true;
}
struct Prepared {
    AssetTransferReport report;
    ~Prepared() { discardAssetTransfer(report); }
};
// Publication owns one validated media file. Rollback removes only that file
// and its now-empty direct directory; unrelated files never enter the removal.
void rollback(const fs::path& root, const fs::path& directory, const fs::path& entry) noexcept {
    try {
        if (directory.parent_path() != root || redirected(root) || redirected(directory) ||
            entry.parent_path() != directory || redirected(entry)) return;
        fs::remove(entry);
        fs::remove(directory);
    } catch (...) {}
}
}

MediaLibrary::MediaLibrary(ProjectSaveStore& store, std::string slot, AssetDomain domain, MediaLibraryLimits limits)
    : store_(store), slot_(std::move(slot)), domain_(domain), limits_(limits) {
    if (domain != AssetDomain::Audio && domain != AssetDomain::Image) throw std::invalid_argument("Media library requires audio or image domain");
    if (!limits.maxSourceBytes || limits.maxSourceBytes > 128ULL * 1024 * 1024 || !limits.maxCacheBytes ||
        limits.maxCacheBytes > 16ULL * 1024 * 1024 * 1024 || !limits.maxEntries || limits.maxEntries > 4096 ||
        !limits.maxScanEntries || limits.maxScanEntries > 1000000)
        throw std::invalid_argument("Invalid media library budgets");
    static_cast<void>(store_.contentDirectory(slot_));
}

fs::path MediaLibrary::location(const MediaLibraryEntry& entry) const {
    return store_.contentDirectory(slot_) / entry.blob / fs::u8path(entry.filename);
}

std::vector<MediaLibraryEntry> MediaLibrary::decode(const Json& value) const {
    if (value.is_null()) return {};
    if (!value.is_array() || value.size() != 2 || value[0] != 1 || !value[1].is_array() || value[1].size() > 4096)
        throw std::invalid_argument("Unsupported or malformed media library version");
    std::vector<MediaLibraryEntry> result;
    std::set<std::string> ids;
    for (const auto& row : value[1]) {
        if (!row.is_array() || row.size() != 7) throw std::invalid_argument("Malformed media library entry");
        for (std::size_t index = 0; index < 6; ++index)
            if (!row[index].is_string()) throw std::invalid_argument("Media library entry requires strings");
        if (!row[6].is_number_unsigned()) throw std::invalid_argument("Media byte count must be unsigned");
        MediaLibraryEntry entry{row[0], row[1], row[2], row[3], row[4], row[5], row[6]};
        if (!identifier(entry.id) || !identifier(entry.blob) || !filename(entry.filename) || entry.title.size() > 4096 ||
            entry.fingerprint.size() > 128 || !entry.bytes || entry.bytes > 128ULL * 1024 * 1024 ||
            !ids.insert(entry.id).second || assetFormatRegistry().require(entry.format).domain != domain_)
            throw std::invalid_argument("Invalid media entry identity, format or path");
        const auto path = location(entry);
        entry.available = !redirected(path) && fs::is_regular_file(path) && fs::file_size(path) == entry.bytes;
        result.push_back(std::move(entry));
    }
    return result;
}

std::vector<MediaLibraryEntry> MediaLibrary::entries() const { return decode(store_.read(slot_)); }
fs::path MediaLibrary::file(const std::string& id) const {
    for (const auto& entry : entries()) if (entry.id == id) {
        if (!entry.available) throw std::runtime_error("Imported media is unavailable");
        return location(entry);
    }
    throw std::invalid_argument("Unknown media entry");
}

MediaLibraryImport MediaLibrary::importFile(const fs::path& input, const AssetCancellation& cancelled) const {
    check(cancelled);
    static_cast<void>(entries());
    const auto source = nativeFilePath(fs::weakly_canonical(nativeFilePath(input)));
    if (!fs::is_regular_file(source) || fs::file_size(source) > limits_.maxSourceBytes)
        throw std::invalid_argument("Media source is missing or exceeds its byte budget");
    const auto& format = assetFormatRegistry().detect(source);
    if (format.domain != domain_) throw std::invalid_argument("Media source belongs to another domain");
    const auto published = store_.contentDirectory(slot_);
    const auto staging = store_.contentDirectory(slot_, SaveContentDirectory::Staging);
    const auto blob = token();
    AssetTransferRequest request;
    request.source = source; request.formatId = format.id;
    request.staging = staging / blob; request.destination = published / blob;
    request.maxSourceBytes = limits_.maxSourceBytes;
    Prepared prepared{prepareAssetTransfer(request, assetFormatRegistry(), cancelled)};
    auto& transfer = prepared.report;
    if (transfer.outputs.size() != 1 || transfer.entry.has_parent_path())
        throw std::runtime_error("Media import requires one self-contained file");
    const auto bytes = fs::file_size(transfer.staging / transfer.entry);
    const auto fingerprint = transfer.generation.outputHashes.at(transfer.entry.generic_u8string());
    const FileLock lock(published.parent_path() / (".media-" + slot_ + ".lock"), std::chrono::seconds(5), cancelled);
    const auto expected = store_.read(slot_);
    auto current = decode(expected);
    MediaLibraryEntry* repair = nullptr;
    for (auto& entry : current) if (entry.fingerprint == fingerprint && entry.bytes == bytes && entry.format == format.id) {
        if (entry.available && sameBytes(location(entry), transfer.staging / transfer.entry, cancelled)) return {entry, true};
        if (!entry.available && !repair) repair = &entry;
    }
    if (!repair && current.size() >= limits_.maxEntries) throw std::runtime_error("Media library exceeds its entry budget");
    const auto used = cacheBytes(published, limits_, cancelled);
    if (bytes > limits_.maxCacheBytes - used) throw std::runtime_error("Media import exceeds the cache byte budget");
    MediaLibraryEntry entry{repair ? repair->id : blob, source.stem().u8string(), format.id, blob,
        transfer.entry.generic_u8string(), fingerprint, bytes, true};
    if (repair) { entry.title = repair->title; *repair = entry; } else current.push_back(entry);
    // Revalidate all serialized fields before any publication.
    const auto payload = encode(current);
    static_cast<void>(decode(payload));
    static_cast<void>(store_.contentDirectory(slot_));
    static_cast<void>(store_.contentDirectory(slot_, SaveContentDirectory::Staging));
    const auto output = publishAssetTransfer(transfer, cancelled);
    try {
        check(cancelled);
        if (!store_.writeIfUnchanged(slot_, expected, payload)) throw std::runtime_error("Media library changed before commit");
    } catch (...) { rollback(published, request.destination, output); throw; }
    return {std::move(entry), false};
}
}
