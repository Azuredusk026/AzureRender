#ifdef _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable: 4701)
#endif
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <stb_vorbis.c>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "assets/audio/AudioDeviceNotifications.hpp"
#include "assets/audio/AudioNotificationQueue.hpp"
#include <map>
#include <string>

namespace azurerender {
#if defined(MA_WIN32_DESKTOP) && !defined(MA_NO_WASAPI)
static_assert(MA_VERSION_MAJOR==0&&MA_VERSION_MINOR==11&&MA_VERSION_REVISION==25,"Review WASAPI notification integration when upgrading miniaudio");
namespace {
std::mutex notificationMutex;
std::map<ma_IMMNotificationClient*,std::shared_ptr<AudioNotificationQueue>> notificationQueues;
template<class F> HRESULT enqueueNotification(ma_IMMNotificationClient* client,F callback){
    try{
        std::shared_ptr<AudioNotificationQueue> queue;
        {std::lock_guard<std::mutex> lock(notificationMutex);const auto found=notificationQueues.find(client);if(found==notificationQueues.end())return S_OK;queue=found->second;}
        queue->post([callback]{const auto result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);try{callback();}catch(...){if(SUCCEEDED(result))CoUninitialize();throw;}if(SUCCEEDED(result))CoUninitialize();});return S_OK;
    }catch(...){return E_OUTOFMEMORY;}
}
HRESULT STDMETHODCALLTYPE deferredState(ma_IMMNotificationClient* client,const WCHAR* id,DWORD state){
    try{const auto value=id?std::wstring(id):std::wstring();return enqueueNotification(client,[client,value,state]{ma_IMMNotificationClient_OnDeviceStateChanged(client,value.c_str(),state);});}catch(...){return E_OUTOFMEMORY;}
}
HRESULT STDMETHODCALLTYPE deferredAdded(ma_IMMNotificationClient* client,const WCHAR* id){
    try{const auto value=id?std::wstring(id):std::wstring();return enqueueNotification(client,[client,value]{ma_IMMNotificationClient_OnDeviceAdded(client,value.c_str());});}catch(...){return E_OUTOFMEMORY;}
}
HRESULT STDMETHODCALLTYPE deferredRemoved(ma_IMMNotificationClient* client,const WCHAR* id){
    try{const auto value=id?std::wstring(id):std::wstring();return enqueueNotification(client,[client,value]{ma_IMMNotificationClient_OnDeviceRemoved(client,value.c_str());});}catch(...){return E_OUTOFMEMORY;}
}
HRESULT STDMETHODCALLTYPE deferredDefault(ma_IMMNotificationClient* client,ma_EDataFlow flow,ma_ERole role,const WCHAR* id){
    try{const bool present=id!=nullptr;const auto value=present?std::wstring(id):std::wstring();return enqueueNotification(client,[client,value,present,flow,role]{ma_IMMNotificationClient_OnDefaultDeviceChanged(client,flow,role,present?value.c_str():nullptr);});}catch(...){return E_OUTOFMEMORY;}
}
HRESULT STDMETHODCALLTYPE deferredProperty(ma_IMMNotificationClient* client,const WCHAR* id,const PROPERTYKEY key){
    try{const auto value=id?std::wstring(id):std::wstring();return enqueueNotification(client,[client,value,key]{ma_IMMNotificationClient_OnPropertyValueChanged(client,value.c_str(),key);});}catch(...){return E_OUTOFMEMORY;}
}
struct NotificationInstaller {
    NotificationInstaller(){g_maNotificationCientVtbl.OnDeviceStateChanged=deferredState;g_maNotificationCientVtbl.OnDeviceAdded=deferredAdded;g_maNotificationCientVtbl.OnDeviceRemoved=deferredRemoved;g_maNotificationCientVtbl.OnDefaultDeviceChanged=deferredDefault;g_maNotificationCientVtbl.OnPropertyValueChanged=deferredProperty;}
} notificationInstaller;
class WasapiNotificationGuard final:public AudioDeviceNotificationGuard {
    ma_IMMNotificationClient* client_;
    std::shared_ptr<AudioNotificationQueue> queue_=std::make_shared<AudioNotificationQueue>();
public:
    explicit WasapiNotificationGuard(ma_device* device):client_(&device->wasapi.notificationClient){std::lock_guard<std::mutex> lock(notificationMutex);notificationQueues.emplace(client_,queue_);}
    void shutdown()override{queue_->close();}
    ~WasapiNotificationGuard()override{queue_->close();std::lock_guard<std::mutex> lock(notificationMutex);notificationQueues.erase(client_);}
};
}
#endif
std::unique_ptr<AudioDeviceNotificationGuard> deferAudioDeviceNotifications(ma_device* device){
#if defined(MA_WIN32_DESKTOP) && !defined(MA_NO_WASAPI)
    if(device&&device->pContext&&device->pContext->backend==ma_backend_wasapi)return std::make_unique<WasapiNotificationGuard>(device);
#else
    static_cast<void>(device);
#endif
    return {};
}
}
