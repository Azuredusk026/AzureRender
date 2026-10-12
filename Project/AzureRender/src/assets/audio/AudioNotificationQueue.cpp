#include "assets/audio/AudioNotificationQueue.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
namespace azurerender {
AudioNotificationQueue::AudioNotificationQueue():worker_([this]{run();}){}
AudioNotificationQueue::~AudioNotificationQueue(){close();}
bool AudioNotificationQueue::post(std::function<void()> callback){
    std::lock_guard<std::mutex> lock(mutex_);if(closing_||!callback||pending_.size()>=64)return false;
    pending_.push_back(std::move(callback));changed_.notify_one();return true;
}
void AudioNotificationQueue::close(){
    {std::lock_guard<std::mutex> lock(mutex_);closing_=true;pending_.clear();changed_.notify_one();}
    if(worker_.joinable())worker_.join();
}
void AudioNotificationQueue::run(){
    for(;;){std::function<void()> callback;
        {std::unique_lock<std::mutex> lock(mutex_);changed_.wait(lock,[this]{return closing_||!pending_.empty();});if(closing_)return;callback=std::move(pending_.front());pending_.pop_front();}
        try{callback();}catch(const std::exception& error){RuntimeDiagnostics::instance().warning("audio-notification",error.what());}
        catch(...){RuntimeDiagnostics::instance().warning("audio-notification","Unknown notification callback failure");}
    }
}
}
