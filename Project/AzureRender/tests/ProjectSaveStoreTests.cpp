#include "runtime/ProjectSaveStore.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using azurerender::ProjectSaveStore;
using Json = nlohmann::json;

void check(bool condition, const char* failure) {
    if (!condition) throw std::runtime_error(failure);
}

int main(int argc, char** argv) {
    try {
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
        first.write("pomodoro", pref);
        check(other.read("pomodoro").is_null(), "Distinct project identities must have distinct slots");
        check(ProjectSaveStore(root, "Room/Case").read("pomodoro") == pref, "Second store must load the exact value");
        check(first.read("settings").is_null(), "Different slot must remain independent");
        const auto updated = Json::array({3, 28, 6});
        first.write("pomodoro", updated);
        check(first.read("pomodoro") == updated, "Existing slot must be replaceable");
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
