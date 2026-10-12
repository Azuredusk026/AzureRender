#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
namespace azurerender {
// One owner closes the queue before destroying the objects callbacks reference.
class AudioNotificationQueue {
public:
    AudioNotificationQueue();
    ~AudioNotificationQueue();
    bool post(std::function<void()> callback);
    void close();
    AudioNotificationQueue(const AudioNotificationQueue&)=delete;
    AudioNotificationQueue& operator=(const AudioNotificationQueue&)=delete;
private:
    void run();
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<std::function<void()>> pending_;
    bool closing_=false;
    std::thread worker_;
};
}
