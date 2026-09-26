#include "render/TransientResourcePool.hpp"

#include <cassert>

int main() {
    azurerender::TransientResourcePool pool;
    const azurerender::TransientResourceKey key{"scene-color", 1280, 720, 1};
    const auto first = pool.acquire(key, 1);
    const auto blocked = pool.acquire(key, 2);
    assert(first.id != blocked.id);
    assert(pool.liveCount() == 2);
    pool.retireFrame(1);
    const auto reused = pool.acquire(key, 3);
    assert(reused.id == first.id);
    const auto other = pool.acquire({"scene-color", 1920, 1080, 1}, 3);
    assert(other.id != reused.id);
    pool.clear();
    assert(pool.liveCount() == 0);
}
