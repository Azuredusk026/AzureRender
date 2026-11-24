#pragma once

#include <condition_variable>
#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <memory>
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
        dispatch(std::move(tasks), false);
    }
    // The caller reserves the first task and uses its own command-pool slot.
    void runWithCaller(std::vector<Task> tasks) {
        dispatch(std::move(tasks), true);
    }
    // Small batches keep secondary-buffer recording on the caller, avoiding
    // worker wakeup cost. Larger batches retain caller/worker overlap.
    void runAdaptive(std::vector<Task> tasks) {
        if (tasks.size() > 5) { runWithCaller(std::move(tasks)); return; }
        if (tasks.empty()) { runWithCaller({}); return; }
        runWithCaller({[tasks = std::move(tasks)](std::size_t caller) {
            std::exception_ptr failure;
            for (const auto& task : tasks) {
                try { task(caller); }
                catch (...) { if (!failure) failure = std::current_exception(); }
            }
            if (failure) std::rethrow_exception(failure);
        }});
    }
    [[nodiscard]] std::size_t recordingThreadCount() const noexcept {
        return workers_.size() + 1;
    }
    [[nodiscard]] std::size_t workerWakeBatches() const noexcept { return generation_; }
private:
    struct Batch {
        explicit Batch(std::vector<Task> work, bool participate)
            : tasks(std::move(work)), failures(tasks.size()),
              remaining(tasks.size()), next(participate ? 1 : 0) {}
        const std::vector<Task> tasks;
        std::vector<std::exception_ptr> failures;
        std::atomic<std::size_t> remaining;
        std::atomic<std::size_t> next;
    };
    void dispatch(std::vector<Task> tasks, bool participate) {
        if (std::this_thread::get_id() != owner_)
            throw std::logic_error("Recording dispatch must run on its owner thread");
        std::unique_lock<std::mutex> lock(mutex_);
        if (dispatching_) throw std::logic_error("Recording dispatch cannot be nested");
        if (tasks.empty()) return;
        if (participate && tasks.size() == 1) {
            dispatching_ = true;
            lock.unlock();
            try { tasks.front()(workers_.size()); }
            catch (...) {
                lock.lock(); dispatching_ = false; throw;
            }
            lock.lock(); dispatching_ = false;
            return;
        }
        const auto batch = std::make_shared<Batch>(std::move(tasks), participate);
        dispatching_ = true;
        active_ = batch;
        ++generation_;
        lock.unlock();
        ready_.notify_all();
        if (participate) {
            executeTask(batch, 0, workers_.size());
            executeBatch(batch, workers_.size());
        }
        lock.lock();
        done_.wait(lock, [&] { return batch->remaining.load(std::memory_order_acquire) == 0; });
        active_.reset();
        dispatching_ = false;
        for (const auto& failure : batch->failures)
            if (failure) std::rethrow_exception(failure);
    }
    void executeTask(const std::shared_ptr<Batch>& batch, std::size_t index, std::size_t worker) {
        try { batch->tasks[index](worker); }
        catch (...) { batch->failures[index] = std::current_exception(); }
        if (batch->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(mutex_);
            done_.notify_one();
        }
    }
    void executeBatch(const std::shared_ptr<Batch>& batch, std::size_t worker) {
        for (;;) {
            const auto index = batch->next.fetch_add(1, std::memory_order_relaxed);
            if (index >= batch->tasks.size()) return;
            executeTask(batch, index, worker);
        }
    }
    void work(std::size_t worker) {
        std::size_t seen = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            ready_.wait(lock, [&] { return stopping_ || generation_ != seen; });
            if (stopping_) return;
            seen = generation_;
            // Late workers retain the previous batch while the caller publishes
            // the next frame. Returning never invalidates their task storage.
            const auto batch = active_;
            lock.unlock();
            if (batch) executeBatch(batch, worker);
            lock.lock();
        }
    }
    void stop() noexcept {
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
        ready_.notify_all();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
    }
    std::vector<std::thread> workers_;
    std::shared_ptr<Batch> active_;
    std::mutex mutex_;
    std::condition_variable ready_, done_;
    std::size_t generation_ = 0;
    bool stopping_ = false;
    bool dispatching_ = false;
    const std::thread::id owner_;
};

}  // namespace azurerender
