#pragma once
#include <memory>
struct ma_device;
namespace azurerender {
class AudioDeviceNotificationGuard {
public:
    virtual ~AudioDeviceNotificationGuard()=default;
    virtual void shutdown()=0;
};
// Miniaudio implementation owns platform types and notification callback wrappers.
std::unique_ptr<AudioDeviceNotificationGuard> deferAudioDeviceNotifications(ma_device* device);
}
