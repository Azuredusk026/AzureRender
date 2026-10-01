#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace azurerender {

// Dispatch is owned by the render thread. Every worker keeps a stable index.
class RecordingWorkerPool final {
public:
    using Task = std::function<void(std::size_t)>;
    explicit RecordingWorkerPool(std::size_t count)
        : owner_(std::this_thread::get_id()) {
        if (count == 0) throw std::invalid_argument("Recording workers must be nonzero");
        try {
            for (std::size_t i = 0; i < count; ++i)
                workers_.emplace_back([this, i] { work(i); });
        } catch (...) { stop(); throw; }
    }
    ~RecordingWorkerPool() { stop(); }
    RecordingWorkerPool(const RecordingWorkerPool&) = delete;
    RecordingWorkerPool& operator=(const RecordingWorkerPool&) = delete;

    void run(std::vector<Task> tasks) {
        if (std::this_thread::get_id() != owner_)
            throw std::logic_error("Recording dispatch must run on its owner thread");
        if (tasks.empty()) return;
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_ = std::move(tasks);
        failures_.assign(tasks_.size(), {});
        remaining_ = tasks_.size();
        next_ = 0;
        ++generation_;
        ready_.notify_all();
        done_.wait(lock, [this] { return remaining_ == 0; });
        tasks_.clear();
        for (const auto& failure : failures_)
            if (failure) std::rethrow_exception(failure);
    }
private:
    void work(std::size_t worker) {
        std::size_t seen = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            ready_.wait(lock, [&] { return stopping_ || generation_ != seen; });
            if (stopping_) return;
            seen = generation_;
            for (;;) {
                const auto index = next_++;
                if (index >= tasks_.size()) break;
                // Dispatch owns this array until every task has completed.
                // Avoid copying the potentially large captured frame context.
                const auto* task = &tasks_[index];
                lock.unlock();
                std::exception_ptr failure;
                try { (*task)(worker); } catch (...) { failure = std::current_exception(); }
                lock.lock();
                failures_[index] = failure;
                if (--remaining_ == 0) done_.notify_one();
            }
        }
    }
    void stop() noexcept {
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
        ready_.notify_all();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
    }
    std::vector<std::thread> workers_;
    std::vector<Task> tasks_;
    std::vector<std::exception_ptr> failures_;
    std::mutex mutex_;
    std::condition_variable ready_, done_;
    std::size_t remaining_ = 0, generation_ = 0;
    bool stopping_ = false;
    std::size_t next_ = 0;
    const std::thread::id owner_;
};

}  // namespace azurerender
