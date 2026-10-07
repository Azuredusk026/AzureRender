#include "runtime/AudioRuntime.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <miniaudio.h>
#include <cmath>
#include <map>
#include <stdexcept>
namespace azurerender {
struct AudioRuntime::Impl {
    ma_engine engine{};
    struct Sound { ma_sound sound{}; bool resume = false; };
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
    }
    ~Impl(){for(auto& sound:sounds)ma_sound_uninit(&sound.second->sound);ma_engine_uninit(&engine);}
};
AudioRuntime::AudioRuntime(bool device):impl_(std::make_unique<Impl>(device)){}
AudioRuntime::~AudioRuntime()=default;
AudioRuntime::Handle AudioRuntime::load(const std::filesystem::path& path,bool loop,float volume) {
    if(!std::isfinite(volume)||volume<0||volume>1)throw std::invalid_argument("Audio volume must be within [0,1]");
    auto sound=std::make_unique<Impl::Sound>();
    if(ma_sound_init_from_file(&impl_->engine,path.string().c_str(),MA_SOUND_FLAG_DECODE,nullptr,nullptr,&sound->sound)!=MA_SUCCESS)
        throw std::runtime_error("Cannot decode audio: "+path.string());
    ma_sound_set_looping(&sound->sound,loop?MA_TRUE:MA_FALSE);ma_sound_set_volume(&sound->sound,volume);
    const auto handle=impl_->next++;impl_->sounds.emplace(handle,std::move(sound));return handle;
}
void AudioRuntime::play(Handle handle){auto& sound=*impl_->sounds.at(handle);ma_sound_seek_to_pcm_frame(&sound.sound,0);if(impl_->paused)sound.resume=true;else if(ma_sound_start(&sound.sound)!=MA_SUCCESS)throw std::runtime_error("Audio playback failed");}
void AudioRuntime::stop(Handle handle){auto& sound=*impl_->sounds.at(handle);ma_sound_stop(&sound.sound);sound.resume=false;}
void AudioRuntime::release(Handle handle){auto it=impl_->sounds.find(handle);if(it!=impl_->sounds.end()){ma_sound_uninit(&it->second->sound);impl_->sounds.erase(it);}}
bool AudioRuntime::playing(Handle handle)const{auto it=impl_->sounds.find(handle);return it!=impl_->sounds.end()&&ma_sound_is_playing(&it->second->sound);}
void AudioRuntime::pauseAll(bool paused){if(impl_->paused==paused)return;impl_->paused=paused;for(auto& item:impl_->sounds){auto& sound=*item.second;if(paused){sound.resume=ma_sound_is_playing(&sound.sound)!=0;ma_sound_stop(&sound.sound);}else if(sound.resume){ma_sound_start(&sound.sound);sound.resume=false;}}}
bool AudioRuntime::hasDevice()const{return impl_->device;}
std::vector<float> AudioRuntime::mix(std::uint32_t frames){if(impl_->device)throw std::logic_error("Offline mix requires no audio device");if(frames>480000)throw std::invalid_argument("Audio mix exceeds frame limit");std::vector<float> samples(static_cast<std::size_t>(frames)*2);if(ma_engine_read_pcm_frames(&impl_->engine,samples.data(),frames,nullptr)!=MA_SUCCESS)throw std::runtime_error("Audio mix failed");return samples;}
}
