#include "runtime/MediaLibrary.hpp"
#include "assets/image/ImageEncoder.hpp"
#include "platform/NativeArguments.hpp"
#include "platform/ProcessRunner.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace azurerender;
namespace fs = std::filesystem;
using Json = nlohmann::json;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& action, const char* message) {
    bool rejected = false; try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, message);
}
void wave(const fs::path& path, unsigned sample) {
    std::ofstream file(path, std::ios::binary);
    auto number = [&](unsigned value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) file.put(static_cast<char>(value >> (8 * i)));
    };
    file.write("RIFF", 4); number(36 + 160, 4); file.write("WAVEfmt ", 8); number(16, 4);
    number(1, 2); number(1, 2); number(8000, 4); number(16000, 4); number(2, 2); number(16, 2);
    file.write("data", 4); number(160, 4);
    for (int i = 0; i < 80; ++i) number(sample, 2);
}
std::size_t children(const fs::path& path) {
    return fs::exists(path) ? static_cast<std::size_t>(std::distance(fs::directory_iterator(path), fs::directory_iterator())) : 0;
}
int main(int argc, char** argv) try {
    const auto args = nativeArguments(argc, argv);
    if (args.size() == 4 && (args[1] == "--import" || args[1] == "--import-image")) {
        ProjectSaveStore store(fs::u8path(args[2]), "process");
        MediaLibrary library(store, args[1] == "--import" ? "music" : "pictures",
            args[1] == "--import" ? AssetDomain::Audio : AssetDomain::Image);
        const auto imported = library.importFile(fs::u8path(args[3]));
        std::cout << imported.entry.format << ": " << library.entries().size() << " committed entries\n";
        return 0;
    }
    require(args.size() == 2, "Expected a managed fixture directory");
    const auto root = fs::absolute(fs::u8path(args[1])) /
        ("case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "source");
    const auto first = root / "source/first.wav", second = root / "source/second.wav", copy = root / "source/copy.wav";
    wave(first, 100); wave(second, 200); fs::copy_file(first, copy);
    const auto firstHash = assetTransferFingerprint(first);
    const auto picture = root / "source/picture.png";
    const unsigned char pixels[]{255, 0, 0, 255, 0, 255, 0, 255};
    writeRgba8Png(picture, 2, 1, pixels);
    ProjectSaveStore store(root / "saves", "Room"), other(root / "saves", "Other");
    MediaLibrary music(store, "music", AssetDomain::Audio), images(store, "pictures", AssetDomain::Image);
    require(music.entries().empty() && !fs::exists(root / "saves"), "Library queries must preserve missing storage");
    const auto imported = music.importFile(first);
    require(!imported.reused && imported.entry.available && imported.entry.title == "first", "Audio import must expose its committed entry");
    const auto cached = music.file(imported.entry.id);
    require(fs::is_regular_file(cached) && assetTransferFingerprint(cached) == firstHash, "Published audio must retain exact source bytes");
    const auto duplicate = music.importFile(copy);
    require(duplicate.reused && duplicate.entry.id == imported.entry.id && music.entries().size() == 1, "Equal bytes must reuse one library entry");
    require(MediaLibrary(other, "music", AssetDomain::Audio).entries().empty(), "Projects must have independent libraries");
    require(images.importFile(picture).entry.available && images.entries().size() == 1, "Images must use the same persistent contract");
    rejects([&] { music.importFile(picture); }, "Audio libraries must reject image content");
    std::ofstream(root / "source/broken.wav") << "RIFF broken";
    const auto before = store.read("music");
    rejects([&] { music.importFile(root / "source/broken.wav"); }, "Corrupt content must fail production decoding");
    rejects([&] { music.importFile(second, [] { return true; }); }, "Cancelled import must fail before publication");
    require(store.read("music") == before, "Rejected imports must preserve committed metadata");
    MediaLibraryLimits small; small.maxSourceBytes = 100;
    rejects([&] { MediaLibrary(store, "small", AssetDomain::Audio, small).importFile(first); }, "Source size budget must be enforced");
    small = {}; small.maxCacheBytes = 100;
    rejects([&] { MediaLibrary(store, "small", AssetDomain::Audio, small).importFile(first); }, "Cache size budget must be enforced");
    small = {}; small.maxEntries = 1;
    MediaLibrary limited(store, "limited", AssetDomain::Audio, small);
    limited.importFile(first);
    rejects([&] { limited.importFile(second); }, "Library entry budget must be enforced");
    require(limited.entries().size() == 1 && children(store.contentDirectory("limited")) == 1, "Budget failure must preserve one committed item");
    small = {}; small.maxScanEntries = 1;
    MediaLibrary scanLimited(store, "scanned", AssetDomain::Audio, small);
    scanLimited.importFile(first);
    rejects([&] { scanLimited.importFile(second); }, "Cache enumeration must enforce its entry budget");
    require(scanLimited.entries().size() == 1, "Scan budget failure must preserve metadata");
    ProjectSaveStore sessionStore;
    rejects([&] { MediaLibrary session(sessionStore, "music", AssetDomain::Audio); }, "Session content must require a persistent owner");
    small = {}; small.maxEntries = 0;
    rejects([&] { MediaLibrary invalid(store, "invalid", AssetDomain::Audio, small); }, "Zero budgets must be rejected");
    auto malformed = before; malformed[1][0][4] = "../outside.wav";
    store.write("malformed", malformed);
    rejects([&] { static_cast<void>(MediaLibrary(store, "malformed", AssetDomain::Audio).entries()); }, "Stored media paths must reject traversal");
    require(store.read("malformed") == malformed, "Invalid metadata queries must preserve the stored payload");
    // Missing sources do not invalidate the owned imported copy.
    fs::remove(first);
    require(MediaLibrary(store, "music", AssetDomain::Audio).file(imported.entry.id) == cached, "Reopening must use the imported copy");
    require(assetTransferFingerprint(cached) == firstHash, "Source deletion must preserve cached media");
    fs::remove(cached);
    require(!music.entries().front().available, "Missing cache files must remain observable as unavailable entries");
    const auto repaired = music.importFile(copy);
    require(repaired.entry.id == imported.entry.id && repaired.entry.available, "Reimport must repair missing content without changing entry identity");

    store.write("future", Json::array({99, Json::array()}));
    MediaLibrary future(store, "future", AssetDomain::Audio);
    const auto futureBytes = store.read("future");
    rejects([&] { future.importFile(second); }, "Newer content versions must reject import");
    require(store.read("future") == futureBytes && children(store.contentDirectory("future")) == 0, "Future payload bytes must remain preserved");
    MediaLibrary changed(store, "changed", AssetDomain::Audio);
    bool changedOnce = false;
    rejects([&] { changed.importFile(second, [&] {
        if (!changedOnce && children(store.contentDirectory("changed", SaveContentDirectory::Staging))) {
            changedOnce = true; store.write("changed", Json::array({99, Json::array()}));
        }
        return false;
    }); }, "A version change during preparation must reject publication");
    require(changedOnce && store.read("changed")[0] == 99 && children(store.contentDirectory("changed")) == 0,
        "Late version changes must preserve metadata and avoid publication");
    MediaLibrary rollback(store, "rollback", AssetDomain::Audio);
    bool fault = false;
    rejects([&] { rollback.importFile(second, [&] {
        if (!fault && children(store.contentDirectory("rollback"))) {
            fault = true;
            const auto slot = store.contentDirectory("rollback").parent_path() / "save-rollback.json";
            std::ofstream(slot, std::ios::binary) << R"({"schemaVersion":99,"payload":[99]})";
        }
        return false;
    }); }, "Save failure after publication must reject the transaction");
    require(fault && children(store.contentDirectory("rollback")) == 0, "Failed metadata commits must release their own new publication");
    const auto rollbackSlot = store.contentDirectory("rollback").parent_path() / "save-rollback.json";
    std::ifstream readBack(rollbackSlot); const std::string preserved((std::istreambuf_iterator<char>(readBack)), {});
    require(preserved == R"({"schemaVersion":99,"payload":[99]})", "Newer outer version must retain exact bytes");
    MediaLibrary cancelPublished(store, "cancel-published", AssetDomain::Audio);
    rejects([&] { cancelPublished.importFile(second, [&] {
        return children(store.contentDirectory("cancel-published")) != 0;
    }); }, "Cancellation after publication must roll back before metadata commit");
    require(store.read("cancel-published").is_null() && children(store.contentDirectory("cancel-published")) == 0,
        "Cancelled publication must leave no visible media entry");
    MediaLibrary stale(store, "stale", AssetDomain::Audio);
    bool replaced = false;
    const auto independentlyCommitted = Json::array({1, Json::array()});
    rejects([&] { stale.importFile(second, [&] {
        if (!replaced && children(store.contentDirectory("stale"))) {
            replaced = true; store.write("stale", independentlyCommitted);
        }
        return false;
    }); }, "A changed metadata snapshot must reject a stale import commit");
    require(replaced && store.read("stale") == independentlyCommitted && children(store.contentDirectory("stale")) == 0,
        "Stale transaction rollback must preserve independently committed data");

    MediaLibrary concurrent(store, "concurrent", AssetDomain::Audio);
    auto one = std::async(std::launch::async, [&] { return concurrent.importFile(copy); });
    auto two = std::async(std::launch::async, [&] { return MediaLibrary(store, "concurrent", AssetDomain::Audio).importFile(second); });
    one.get(); two.get();
    require(concurrent.entries().size() == 2, "Concurrent library instances must merge independent imports");
#ifdef _WIN32
    ProcessRequest request; request.executable = fs::absolute(fs::u8path(args.front())); request.timeoutMs = 20000;
    request.arguments = {"--import", (root / "saves").u8string(), copy.u8string()};
    auto peer = request; peer.arguments.back() = second.u8string();
    const std::atomic<bool> cancelled{false};
    auto processOne = std::async(std::launch::async, [&] { return runProcess(request, cancelled); });
    auto processTwo = std::async(std::launch::async, [&] { return runProcess(peer, cancelled); });
    const auto p1 = processOne.get(), p2 = processTwo.get();
    require(p1.passed && p2.passed, (p1.output + p2.output + p1.diagnostic + p2.diagnostic).c_str());
    ProjectSaveStore processStore(root / "saves", "process");
    require(MediaLibrary(processStore, "music", AssetDomain::Audio).entries().size() == 2, "Processes must preserve both imported entries");
#endif
    require(assetTransferFingerprint(copy) == firstHash, "Import must preserve source bytes");
    std::cout << "Media library audio/image decode, persistence, deduplication, budgets, versions, rollback and concurrency passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
