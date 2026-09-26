#include "render/FrameTaskScheduler.hpp"
#include <cassert>
#include <vector>

int main() {
    azurerender::FrameTaskScheduler scheduler;
    std::vector<int> order;
    scheduler.add(2, [&] { order.push_back(2); });
    scheduler.add(1, [&] { order.push_back(1); });
    scheduler.add(2, [&] { order.push_back(3); });
    scheduler.runDeterministic();
    assert((order == std::vector<int>{1, 2, 3}));
}
