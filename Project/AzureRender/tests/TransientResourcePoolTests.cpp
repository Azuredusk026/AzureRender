#include "render/TransientResourcePool.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        azurerender::rhi::NullGpuAllocator allocator;
        azurerender::TransientResourcePool pool(allocator);
        const azurerender::TransientResourceKey key{
            "scene-color", 1280, 720, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
        const auto first = pool.acquireImage(key, 1);
        const auto blocked = pool.acquireImage(key, 2);
        check(first.image.image != blocked.image.image, "in-flight images cannot alias");
        check(allocator.statistics().liveImages == 2, "pool owns actual images");
        pool.retireFrame(1);
        const auto reused = pool.acquireImage(key, 3);
        check(reused.image.image == first.image.image, "completed image is reused");
        const auto capture = pool.acquireBuffer(4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, 3);
        check(capture.buffer.mapped != nullptr, "capture buffer is mapped");
        const auto pending = pool.acquireBuffer(4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, 4);
        check(capture.buffer.buffer != pending.buffer.buffer, "in-flight capture cannot alias");
        pool.retireFrame(3);
        const auto captureAgain = pool.acquireBuffer(4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, 5);
        check(capture.buffer.buffer == captureAgain.buffer.buffer, "completed capture buffer is reused");
        pool.trim();
        check(allocator.statistics().liveImages == 0, "trim releases retired image sizes");
        check(allocator.statistics().liveBuffers == 2, "trim preserves in-flight buffers");
        pool.retireFrame(5);
        pool.clear();
        check(allocator.statistics().liveImages == 0 && allocator.statistics().liveBuffers == 0,
            "shutdown releases all GPU allocations");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    return 0;
}
