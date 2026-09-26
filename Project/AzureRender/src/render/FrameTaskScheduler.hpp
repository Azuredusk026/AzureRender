#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace azurerender {

class FrameTaskScheduler final {
public:
    using Task = std::function<void()>;
    void add(std::uint32_t order, Task task) { tasks_.push_back({order, std::move(task)}); }
    void runDeterministic() {
        std::stable_sort(tasks_.begin(), tasks_.end(), [](const Entry& a, const Entry& b) { return a.order < b.order; });
        for (auto& entry : tasks_) entry.task();
        tasks_.clear();
    }
private:
    struct Entry { std::uint32_t order; Task task; };
    std::vector<Entry> tasks_;
};

}  // namespace azurerender
