#include "foundation/FileLock.hpp"
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("Expected a managed fixture directory");
        const auto root = std::filesystem::absolute(argv[1])
            / ("case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        const auto path = root / "commit.lock";
        {
            azurerender::FileLock owner(path);
            auto waiter = std::async(std::launch::async, [&] {
                try { azurerender::FileLock blocked(path, 20ms); }
                catch (const std::runtime_error& error) { return std::string(error.what()).find("timed out") != std::string::npos; }
                return false;
            });
            if (!waiter.get()) throw std::runtime_error("A held file lock must enforce its deadline");
            auto reader = std::async(std::launch::async, [&] {
                try { azurerender::FileLock blocked(path, azurerender::FileLockMode::Shared, 20ms); }
                catch (const std::runtime_error&) { return true; }
                return false;
            });
            if (!reader.get()) throw std::runtime_error("Exclusive writers must exclude shared readers");
            bool cancelled = false;
            try { azurerender::FileLock blocked(path, 1s, [] { return true; }); }
            catch (const std::runtime_error& error) { cancelled = std::string(error.what()).find("cancelled") != std::string::npos; }
            if (!cancelled) throw std::runtime_error("File lock waits must observe cancellation");
        }
        { azurerender::FileLock reopened(path, 20ms); }
        {
            azurerender::FileLock reader(path, azurerender::FileLockMode::Shared, 20ms);
            auto peer = std::async(std::launch::async, [&] {
                azurerender::FileLock other(path, azurerender::FileLockMode::Shared, 20ms);
                return true;
            });
            if (!peer.get()) throw std::runtime_error("Shared readers must coexist");
            auto writer = std::async(std::launch::async, [&] {
                try { azurerender::FileLock blocked(path, 20ms); }
                catch (const std::runtime_error&) { return true; }
                return false;
            });
            if (!writer.get()) throw std::runtime_error("Shared readers must exclude a writer");
        }
        const auto missing = root / "missing.lock";
        bool missingRejected = false;
        try { azurerender::FileLock reader(missing, azurerender::FileLockMode::Shared, 20ms); }
        catch (const std::exception&) { missingRejected = true; }
        if (!missingRejected || std::filesystem::exists(missing))
            throw std::runtime_error("Shared acquisition must preserve missing files");
        std::cout << "File lock exclusion, deadline, cancellation and native handle release passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
