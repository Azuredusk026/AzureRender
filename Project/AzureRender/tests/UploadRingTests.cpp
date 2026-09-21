#include "rhi/RingFrameAllocator.hpp"

#include <cassert>
#include <stdexcept>

namespace {

using azurerender::rhi::RingFrameAllocator;

template <typename Exception, typename Fn>
bool throws(Fn&& fn) {
    try {
        fn();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    // Construction validation.
    assert(throws<std::invalid_argument>(
        [] { RingFrameAllocator bad(0, 2, 256); }));
    assert(throws<std::invalid_argument>(
        [] { RingFrameAllocator bad(1024, 0, 256); }));
    assert(throws<std::invalid_argument>(
        [] { RingFrameAllocator bad(1024, 2, 300); }));

    RingFrameAllocator ring(1024, 2, 256);
    assert(ring.totalSize() == 2048);
    assert(ring.perFrameCapacity() == 1024);

    // Offsets honor the configured alignment and pack inside the window.
    ring.beginFrame(0);
    assert(ring.allocate(100) == 0);
    assert(ring.allocate(100) == 256);
    assert(ring.allocate(512) == 512);
    // Exactly at the window end is still valid; one more byte overflows.
    assert(throws<std::overflow_error>(
        [&] { static_cast<void>(ring.allocate(1)); }));

    // A larger caller alignment overrides the ring alignment.
    ring.beginFrame(0);
    assert(ring.allocate(8) == 0);
    assert(ring.allocate(8, 512) == 512);

    // beginFrame resets the window so the next frame reuses its own segment.
    ring.beginFrame(0);
    assert(ring.allocate(8) == 0);

    // Frame windows are isolated segments of one buffer.
    ring.beginFrame(1);
    assert(ring.allocate(100) == 1024);
    assert(ring.allocate(1) == 1280);
    assert(ring.allocate(512) == 1536);
    assert(throws<std::overflow_error>(
        [&] { static_cast<void>(ring.allocate(1)); }));

    // Contract violations are loud, not silent corruption.
    assert(throws<std::out_of_range>([&] { ring.beginFrame(2); }));
    assert(throws<std::invalid_argument>(
        [&] { static_cast<void>(ring.allocate(0)); }));
    assert(throws<std::invalid_argument>(
        [&] { static_cast<void>(ring.allocate(8, 300)); }));

    RingFrameAllocator unconfigured;
    assert(throws<std::logic_error>([&] { unconfigured.beginFrame(0); }));
    return 0;
}
