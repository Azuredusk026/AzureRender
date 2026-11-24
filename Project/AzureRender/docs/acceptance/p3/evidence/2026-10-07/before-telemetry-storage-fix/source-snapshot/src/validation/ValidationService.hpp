#pragma once
#include "runtime/ObservationRegistry.hpp"
#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
namespace azurerender {
struct ValidationCallbacks {
    using Callback=std::function<nlohmann::json(const nlohmann::json&)>;
    Callback edit,input,capture,describe;
};
class ValidationService {
public:
    using Json=nlohmann::json;
    using Clock=std::chrono::steady_clock;
    ValidationService(ObservationRegistry& observations,ValidationCallbacks callbacks={});
    ~ValidationService();
    std::future<Json> submit(Json request);
    void pump(std::uint64_t frame,Clock::time_point now=Clock::now());
    void stop();
    void loadScript(const Json& script);
    bool complete() const;
    Json report() const;
private:
    struct Pending {
        Json request;std::promise<Json> result;
        Clock::time_point queued=Clock::now();
        std::optional<std::uint64_t> firstFrame;
    };
    ObservationRegistry& observations_;ValidationCallbacks callbacks_;
    mutable std::mutex mutex_;
    std::deque<std::shared_ptr<Pending>> pending_;
    bool stopped_=false;
    std::thread::id owner_=std::this_thread::get_id();
    Json steps_=Json::array(),results_=Json::array();
    std::size_t cursor_=0;
    std::future<Json> step_;
    Clock::time_point scriptStart_=Clock::now();
    void checkThread() const;
    std::optional<Json> evaluate(Pending& request,std::uint64_t frame,Clock::time_point now);
};
}
