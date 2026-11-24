#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
namespace azurerender {
class AudioRuntime {
public:
    using Handle = std::uint64_t;
    explicit AudioRuntime(bool device = true);
    ~AudioRuntime();
    Handle load(const std::filesystem::path& path, bool loop, float volume);
    void play(Handle handle);
    void stop(Handle handle);
    void release(Handle handle);
    bool playing(Handle handle) const;
    void pauseAll(bool paused);
    bool hasDevice() const;
    std::vector<float> mix(std::uint32_t frames);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
