#pragma once

#include <algorithm>
#include <atomic>
#include <exception>
#include <future>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace azurerender {

class FrameTaskScheduler final {
public:
    using Task = std::function<void()>;
    using WorkerTask = std::function<void(std::size_t)>;
    void add(std::uint32_t order, Task task) {
        addWorkerTask(order, [task = std::move(task)](std::size_t) { task(); });
    }
    void addWorkerTask(std::uint32_t order, WorkerTask task) {
        tasks_.push_back({order, std::move(task)});
    }
    void runDeterministic() {
        std::stable_sort(tasks_.begin(), tasks_.end(), [](const Entry& a, const Entry& b) { return a.order < b.order; });
        for (auto& entry : tasks_) entry.task(0);
        tasks_.clear();
    }
    // Tasks must read immutable input and write disjoint output. Submission
    // ordering belongs to the caller after every recording task has joined.
    void runParallel(std::size_t workerCount) {
        auto tasks = std::move(tasks_);
        tasks_.clear();
        if (tasks.empty()) return;
        workerCount = std::max<std::size_t>(1, std::min(workerCount, tasks.size()));
        std::atomic<std::size_t> next{0};
        std::vector<std::future<void>> workers;
        std::vector<std::exception_ptr> failures(tasks.size());
        for (std::size_t i = 0; i < workerCount; ++i) {
            workers.push_back(std::async(std::launch::async, [&, i] {
                for (;;) {
                    const auto index = next.fetch_add(1);
                    if (index >= tasks.size()) break;
                    try { tasks[index].task(i); }
                    catch (...) { failures[index] = std::current_exception(); }
                }
            }));
        }
        for (auto& worker : workers) worker.get();
        for (const auto& failure : failures)
            if (failure) std::rethrow_exception(failure);
    }
private:
    struct Entry { std::uint32_t order; WorkerTask task; };
    std::vector<Entry> tasks_;
};

}  // namespace azurerender
