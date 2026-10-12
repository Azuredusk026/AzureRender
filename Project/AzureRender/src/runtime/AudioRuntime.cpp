#include "runtime/AudioRuntime.hpp"
#include "assets/audio/AudioDeviceNotifications.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <miniaudio.h>
#include <cmath>
#include <map>
#include <stdexcept>
namespace azurerender {
struct AudioRuntime::Impl {
    ma_engine engine{};
    std::unique_ptr<AudioDeviceNotificationGuard> notifications;
    struct Sound;
    struct ReaderSource {ma_data_source_base base{};Sound* owner=nullptr;};
    struct Sound {
        ma_sound sound{};AudioDescription source;bool resume=false,initialized=false,buffered=false,streamed=false;
        DecodedAudio audio;ma_audio_buffer buffer{};ReaderSource stream;std::unique_ptr<AudioFrameReader> reader;std::uint64_t cursor=0;
        ~Sound(){if(initialized)ma_sound_uninit(&sound);if(buffered)ma_audio_buffer_uninit(&buffer);if(streamed)ma_data_source_uninit(&stream.base);}
    };
    static Sound& owner(ma_data_source* data){return *reinterpret_cast<ReaderSource*>(data)->owner;}
    static ma_result read(ma_data_source* data,void* output,ma_uint64 frames,ma_uint64* got){try{auto& value=owner(data);*got=value.reader->read(static_cast<float*>(output),frames);value.cursor+=*got;return *got?MA_SUCCESS:MA_AT_END;}catch(const std::exception& error){*got=0;RuntimeDiagnostics::instance().error("audio",DiagnosticCode::Runtime,error.what());return MA_ERROR;}}
    static ma_result seek(ma_data_source* data,ma_uint64 frame){try{auto& value=owner(data);value.reader->seek(frame);value.cursor=frame;return MA_SUCCESS;}catch(const std::exception&){return MA_ERROR;}}
    static ma_result format(ma_data_source* data,ma_format* format,ma_uint32* channels,ma_uint32* rate,ma_channel* map,size_t capacity){const auto& value=owner(data).source;if(format)*format=ma_format_f32;if(channels)*channels=value.channels;if(rate)*rate=value.sampleRate;if(map)ma_channel_map_init_standard(ma_standard_channel_map_default,map,capacity,value.channels);return MA_SUCCESS;}
    static ma_result cursor(ma_data_source* data,ma_uint64* value){*value=owner(data).cursor;return MA_SUCCESS;}
    static ma_result length(ma_data_source* data,ma_uint64* value){*value=owner(data).source.frames;return MA_SUCCESS;}
    static const ma_data_source_vtable* vtable(){static const ma_data_source_vtable table{read,seek,format,cursor,length,nullptr,0};return &table;}
    std::map<Handle,std::unique_ptr<Sound>> sounds;
    Handle next = 1;
    bool device = false, paused = false;
    explicit Impl(bool requested) {
        auto config = ma_engine_config_init();config.channels=2;config.sampleRate=48000;config.noDevice=requested?MA_FALSE:MA_TRUE;
        auto result = ma_engine_init(&config, &engine);
        if (result != MA_SUCCESS && requested) {
            config.noDevice=MA_TRUE;result=ma_engine_init(&config,&engine);
            RuntimeDiagnostics::instance().warning("audio","Audio device unavailable; offline mixer active");
        } else device=requested;
        if (result != MA_SUCCESS) throw std::runtime_error("Audio engine initialization failed");
        try{if(device)notifications=deferAudioDeviceNotifications(ma_engine_get_device(&engine));}
        catch(...){ma_engine_uninit(&engine);throw;}
    }
    ~Impl(){if(notifications)notifications->shutdown();sounds.clear();ma_engine_uninit(&engine);notifications.reset();}
};
AudioRuntime::AudioRuntime(bool device):impl_(std::make_unique<Impl>(device)){}
AudioRuntime::~AudioRuntime()=default;
AudioRuntime::Handle AudioRuntime::load(const std::filesystem::path& path,bool loop,float volume) {
    if(!std::isfinite(volume)||volume<0||volume>1)throw std::invalid_argument("Audio volume must be within [0,1]");
    const auto source=probeAudioFile(path);
    auto sound=std::make_unique<Impl::Sound>();sound->source=source;ma_data_source* data=nullptr;
    if(source.streaming){sound->reader=openAudioFileReader(path);sound->stream.owner=sound.get();auto config=ma_data_source_config_init();config.vtable=Impl::vtable();if(ma_data_source_init(&config,&sound->stream.base)!=MA_SUCCESS)throw std::runtime_error("Audio streaming source initialization failed");sound->streamed=true;data=&sound->stream.base;}
    else {sound->audio=decodeAudioFile(path);auto config=ma_audio_buffer_config_init(ma_format_f32,source.channels,source.frames,sound->audio.samples.data(),nullptr);config.sampleRate=source.sampleRate;if(ma_audio_buffer_init(&config,&sound->buffer)!=MA_SUCCESS)throw std::runtime_error("Audio PCM source initialization failed");sound->buffered=true;data=&sound->buffer;}
    if(ma_sound_init_from_data_source(&impl_->engine,data,0,nullptr,&sound->sound)!=MA_SUCCESS)throw std::runtime_error("Cannot initialize audio: "+path.u8string());sound->initialized=true;
    ma_sound_set_looping(&sound->sound,loop?MA_TRUE:MA_FALSE);ma_sound_set_volume(&sound->sound,volume);
    const auto handle=impl_->next++;impl_->sounds.emplace(handle,std::move(sound));
    try{RuntimeDiagnostics::instance().info("audio",std::string(description(handle).streaming?"Streaming source: ":"Decoded source: ")+path.u8string());}
    catch(...){release(handle);throw;}return handle;
}
void AudioRuntime::play(Handle handle){auto& sound=*impl_->sounds.at(handle);ma_sound_seek_to_pcm_frame(&sound.sound,0);if(impl_->paused)sound.resume=true;else if(ma_sound_start(&sound.sound)!=MA_SUCCESS)throw std::runtime_error("Audio playback failed");}
void AudioRuntime::stop(Handle handle){auto& sound=*impl_->sounds.at(handle);ma_sound_stop(&sound.sound);sound.resume=false;}
void AudioRuntime::release(Handle handle){auto it=impl_->sounds.find(handle);if(it!=impl_->sounds.end()){impl_->sounds.erase(it);}}
bool AudioRuntime::playing(Handle handle)const{auto it=impl_->sounds.find(handle);return it!=impl_->sounds.end()&&ma_sound_is_playing(&it->second->sound);}
AudioDescription AudioRuntime::description(Handle handle)const{
    const auto& entry=*impl_->sounds.at(handle);const auto& sound=entry.sound;AudioDescription result;ma_format format{};ma_uint64 frames=0;
    const auto formatStatus=ma_sound_get_data_format(&sound,&format,&result.channels,&result.sampleRate,nullptr,0);
    const auto lengthStatus=ma_sound_get_length_in_pcm_frames(&sound,&frames);
    if(formatStatus!=MA_SUCCESS||(lengthStatus!=MA_SUCCESS&&lengthStatus!=MA_NOT_IMPLEMENTED))throw std::runtime_error("Cannot describe audio source: format="+std::to_string(formatStatus)+", length="+std::to_string(lengthStatus));
    if(lengthStatus==MA_NOT_IMPLEMENTED||frames==0)frames=static_cast<ma_uint64>(std::llround(entry.source.duration*result.sampleRate));
    result.frames=frames;result.duration=static_cast<double>(frames)/result.sampleRate;
    result.streaming=entry.source.streaming;return result;
}
void AudioRuntime::pauseAll(bool paused){if(impl_->paused==paused)return;impl_->paused=paused;for(auto& item:impl_->sounds){auto& sound=*item.second;if(paused){sound.resume=ma_sound_is_playing(&sound.sound)!=0;ma_sound_stop(&sound.sound);}else if(sound.resume){ma_sound_start(&sound.sound);sound.resume=false;}}}
bool AudioRuntime::hasDevice()const{return impl_->device;}
std::vector<float> AudioRuntime::mix(std::uint32_t frames){if(impl_->device)throw std::logic_error("Offline mix requires no audio device");if(frames>480000)throw std::invalid_argument("Audio mix exceeds frame limit");std::vector<float> samples(static_cast<std::size_t>(frames)*2);if(ma_engine_read_pcm_frames(&impl_->engine,samples.data(),frames,nullptr)!=MA_SUCCESS)throw std::runtime_error("Audio mix failed");return samples;}
}
