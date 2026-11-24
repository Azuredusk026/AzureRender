#include "foundation/FileLock.hpp"
#include "foundation/NativeFilePath.hpp"
#include <stdexcept>
#include <system_error>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace azurerender {
struct FileLock::Impl {
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
    OVERLAPPED overlap{};
#else
    int file = -1;
#endif
    bool held = false;
    ~Impl() {
#ifdef _WIN32
        if (file != INVALID_HANDLE_VALUE) {
            if (held) UnlockFileEx(file, 0, 1, 0, &overlap);
            CloseHandle(file);
        }
#else
        if (file >= 0) { if (held) ::flock(file, LOCK_UN); ::close(file); }
#endif
    }
    void acquire(const std::filesystem::path& path, FileLockMode mode, std::chrono::milliseconds timeout,
                 const std::function<bool()>& cancelled) {
        if (timeout.count() <= 0) throw std::invalid_argument("File lock timeout must be positive");
        if (cancelled && cancelled()) throw std::runtime_error("File lock cancelled");
        const auto native = nativeFilePath(std::filesystem::absolute(path));
#ifdef _WIN32
        file = CreateFileW(native.c_str(), mode == FileLockMode::Exclusive ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, mode == FileLockMode::Exclusive ? OPEN_ALWAYS : OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot open file lock");
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            throw std::runtime_error("File locks require regular files");
#else
        file = ::open(native.c_str(), (mode == FileLockMode::Exclusive ? O_CREAT | O_RDWR : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (file < 0) throw std::system_error(errno, std::generic_category(), "Cannot open file lock");
        struct stat info{};
        if (::fstat(file, &info) != 0 || !S_ISREG(info.st_mode)) throw std::runtime_error("File locks require regular files");
#endif
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            if (cancelled && cancelled()) throw std::runtime_error("File lock cancelled");
#ifdef _WIN32
            if (LockFileEx(file, LOCKFILE_FAIL_IMMEDIATELY | (mode == FileLockMode::Exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0),
                0, 1, 0, &overlap)) { held = true; return; }
            const auto error = GetLastError();
            if (error != ERROR_LOCK_VIOLATION) throw std::system_error(static_cast<int>(error), std::system_category(), "Cannot acquire file lock");
#else
            if (::flock(file, (mode == FileLockMode::Exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0) { held = true; return; }
            if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)
                throw std::system_error(errno, std::generic_category(), "Cannot acquire file lock");
#endif
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("File lock timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
};
FileLock::FileLock(const std::filesystem::path& file, std::chrono::milliseconds timeout,
                   std::function<bool()> cancelled) : FileLock(file, FileLockMode::Exclusive, timeout, std::move(cancelled)) {}
FileLock::FileLock(const std::filesystem::path& file, FileLockMode mode, std::chrono::milliseconds timeout,
                   std::function<bool()> cancelled) : impl_(std::make_unique<Impl>()) {
    impl_->acquire(file, mode, timeout, cancelled);
}
FileLock::~FileLock() = default;
}
