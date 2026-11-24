#include "runtime/ProjectSaveStore.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <future>
#include "platform/NativeArguments.hpp"
#include "platform/ProcessRunner.hpp"

namespace fs = std::filesystem;
using azurerender::ProjectSaveStore;
using Json = nlohmann::json;

void check(bool condition, const char* failure) {
    if (!condition) throw std::runtime_error(failure);
}

int main(int argc, char** argv) {
    try {
        const auto arguments = azurerender::nativeArguments(argc, argv);
        if (arguments.size() == 4 && arguments[1] == "--increment") {
            const ProjectSaveStore store(fs::u8path(arguments[2]), arguments[3]);
            for (int index = 0; index < 25; ++index) {
                bool committed = false;
                for (int retry = 0; retry < 1000 && !committed; ++retry) {
                    const auto previous = store.read("process-counter");
                    committed = store.writeIfUnchanged("process-counter", previous, previous.get<int>() + 1);
                }
                check(committed, "Conditional process update exhausted retries");
            }
            return 0;
        }
        if (argc != 2) throw std::invalid_argument("A managed test directory is required");
        const auto parent = fs::absolute(fs::u8path(argv[1]));
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = parent / ("case-" + std::to_string(nonce));
        const auto first = ProjectSaveStore(root, "Room/Case");
        const auto other = ProjectSaveStore(root, "room/case");
        const auto pref = Json::array({2, 30, 7});
        const ProjectSaveStore preview;
        preview.write("pomodoro", pref);
        check(preview.read("pomodoro") == pref && ProjectSaveStore{}.read("pomodoro").is_null(),
              "Preview state must stay within one script session");
        check(first.read("pomodoro").is_null(), "Missing slot must be empty");
        check(!fs::exists(root), "Read must not create a save directory");
        const auto content = first.contentDirectory("media");
        const auto staging = first.contentDirectory("media", azurerender::SaveContentDirectory::Staging);
        check(content != staging && content.parent_path() == staging.parent_path(), "Content and staging must have separate locations");
        check(content != other.contentDirectory("media"), "Content paths must preserve project identity");
        check(!fs::exists(root), "Content path queries must not create directories");
        bool invalidContent = false;
        try { static_cast<void>(first.contentDirectory("../media")); }
        catch (const std::invalid_argument&) { invalidContent = true; }
        check(invalidContent, "Content directory names must reject traversal");
        bool ephemeralContent = false;
        try { static_cast<void>(preview.contentDirectory("media")); }
        catch (const std::logic_error&) { ephemeralContent = true; }
        check(ephemeralContent, "Session-only saves cannot allocate persistent content directories");
        check(preview.writeIfUnchanged("pomodoro", pref, Json::array({1, 9})), "Session conditional commit must accept its current snapshot");
        check(!preview.writeIfUnchanged("pomodoro", pref, Json::array({1, 10})), "Session conditional commit must reject stale snapshots");
        first.write("pomodoro", pref);
        check(other.read("pomodoro").is_null(), "Distinct project identities must have distinct slots");
        check(ProjectSaveStore(root, "Room/Case").read("pomodoro") == pref, "Second store must load the exact value");
        check(first.read("settings").is_null(), "Different slot must remain independent");
        check(!fs::exists(content.parent_path() / ".save-settings.lock"), "Missing-slot reads must not create lock files");
        const auto updated = Json::array({3, 28, 6});
        first.write("pomodoro", updated);
        check(first.read("pomodoro") == updated, "Existing slot must be replaceable");
        check(first.writeIfUnchanged("revision", nullptr, pref), "Missing persistent slots must support conditional creation");
        check(!first.writeIfUnchanged("revision", nullptr, updated) && first.read("revision") == pref,
              "Persistent conditional commits must reject stale snapshots");
        const auto readOnlyLock = content.parent_path() / ".save-revision.lock";
        fs::permissions(readOnlyLock, fs::perms::owner_read, fs::perm_options::replace);
        check(first.read("revision") == pref, "Snapshot reads must use read-only lock access");
        fs::permissions(readOnlyLock, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
        bool invalidKey = false;
        try { first.write("../bad", pref); } catch (const std::invalid_argument&) { invalidKey = true; }
        check(invalidKey, "Relative traversal must be rejected");
        bool invalidPayload = false;
        try { first.write("pomodoro", std::string(1024 * 1024, 'q')); }
        catch (const std::invalid_argument&) { invalidPayload = true; }
        check(invalidPayload && first.read("pomodoro") == updated, "Large payload must preserve the prior slot");

        auto paths = fs::directory_iterator(root);
        const auto projectFolder = paths->path();
        const auto file = projectFolder / "save-pomodoro.json";
        std::ofstream(file, std::ios::binary | std::ios::trunc) << "{broken";
        check(first.read("pomodoro").is_null(), "Malformed content must use empty fallback");
        first.write("pomodoro", pref);
        check(first.read("pomodoro") == pref, "A valid save must recover the malformed slot");
        std::ofstream(file, std::ios::binary | std::ios::trunc) << R"({"schemaVersion":99,"data":[1]})";
        bool futureVersion = false;
        try { first.write("pomodoro", pref); } catch (const std::invalid_argument&) { futureVersion = true; }
        check(futureVersion, "Unsupported future save version must be preserved");
        check(first.read("pomodoro").is_null(), "Unsupported version must use empty fallback");
        futureVersion = false;
        try { static_cast<void>(first.writeIfUnchanged("pomodoro", nullptr, pref)); }
        catch (const std::invalid_argument&) { futureVersion = true; }
        check(futureVersion, "Conditional commits must preserve future containers");

        std::exception_ptr workerError, mainError;
        std::thread worker([&]{
            try {
                other.write("marker", Json::array({4, 5, 6}));
                ProjectSaveStore(root, "Room/Case").write("shared", pref);
            }
            catch (...) { workerError = std::current_exception(); }
        });
        try {
            first.write("marker", Json::array({0, 1, 2}));
            first.write("shared", updated);
        } catch (...) { mainError = std::current_exception(); }
        worker.join();
        if (mainError) std::rethrow_exception(mainError);
        if (workerError) std::rethrow_exception(workerError);
        check(other.read("marker") == Json::array({4, 5, 6}), "Other project lost its write");
        check(first.read("marker") == Json::array({0, 1, 2}), "First project lost its write");
        const auto shared = first.read("shared");
        check(shared == pref || shared == updated, "Concurrent writes created a partial snapshot");
        first.write("conditional-counter", 0);
        const ProjectSaveStore secondStore(root, "Room/Case");
        std::atomic<int> ready{0}, commits{0};
        std::atomic<bool> publish{false};
        std::exception_ptr firstError, secondError;
        auto attempt = [&](const ProjectSaveStore& store, std::exception_ptr& error) {
            bool posted = false;
            try {
                const auto previous = store.read("conditional-counter");
                ++ready;
                posted = true;
                while (!publish) std::this_thread::yield();
                if (store.writeIfUnchanged("conditional-counter", previous, 1)) ++commits;
            } catch (...) { error = std::current_exception(); if (!posted) ++ready; }
        };
        std::thread firstWriter([&] { attempt(first, firstError); });
        std::thread secondWriter([&] { attempt(secondStore, secondError); });
        while (ready < 2) std::this_thread::yield();
        publish = true;
        firstWriter.join(); secondWriter.join();
        if (firstError) std::rethrow_exception(firstError);
        if (secondError) std::rethrow_exception(secondError);
        check(commits == 1 && first.read("conditional-counter") == 1,
              "Exactly one concurrent commit may consume a snapshot");
        first.write("process-counter", 0);
#ifdef _WIN32
        azurerender::ProcessRequest request;
        request.executable = fs::absolute(fs::u8path(arguments.front()));
        request.arguments = {"--increment", root.u8string(), "Room/Case"};
        request.timeoutMs = 15000;
        const std::atomic<bool> cancelled{false};
        auto processOne = std::async(std::launch::async, [&] { return azurerender::runProcess(request, cancelled); });
        auto processTwo = std::async(std::launch::async, [&] { return azurerender::runProcess(request, cancelled); });
        const auto one = processOne.get(), two = processTwo.get();
        check(one.passed && two.passed, ("Conditional child process failed: " + one.output + two.output + one.diagnostic + two.diagnostic).c_str());
        check(first.read("process-counter") == 50, "Conditional process updates must merge every increment");
#endif
        const Json snapshotA = std::string(128 * 1024, 'a'), snapshotB = std::string(128 * 1024, 'b');
        first.write("live-snapshot", snapshotA);
        std::atomic<bool> reading{false}, finished{false};
        std::exception_ptr readerError, writerError;
        std::thread reader([&] {
            try {
                reading = true;
                while (!finished) {
                    const auto snapshot = first.read("live-snapshot");
                    check(snapshot == snapshotA || snapshot == snapshotB,
                          "Concurrent readers must see a complete published snapshot");
                }
            } catch (...) { readerError = std::current_exception(); }
        });
        while (!reading) std::this_thread::yield();
        try {
            for (int index = 0; index < 20; ++index) first.write("live-snapshot", index % 2 ? snapshotA : snapshotB);
        } catch (...) { writerError = std::current_exception(); }
        finished = true;
        reader.join();
        if (readerError) std::rethrow_exception(readerError);
        if (writerError) std::rethrow_exception(writerError);
#ifdef _WIN32
        const auto deepRoot = root / "nested" / std::string(115, 'd');
        const std::string longProject = "18ab3c8e-b0d8-4fda-968b-a219e4fa4d63";
        const auto deep = ProjectSaveStore(deepRoot, longProject);
        check(deep.read("room-language").is_null(), "Deep paths must support empty slots");
        deep.write("room-language", pref);
        check(ProjectSaveStore(deepRoot, longProject).read("room-language") == pref,
              "Another store must reopen a deep Windows save");
        deep.write("room-language", updated);
        check(deep.read("room-language") == updated, "Deep Windows saves must support atomic replacement");
#endif
        std::cout << "Project save scope, replacement, fallback and repeat open passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
