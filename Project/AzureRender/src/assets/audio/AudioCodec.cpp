#include "assets/audio/AudioCodec.hpp"
#include <miniaudio.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace azurerender {
namespace {
constexpr std::uint64_t sourceBudget=128ULL*1024*1024;
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Audio decode cancelled");}
std::vector<unsigned char> read(const std::filesystem::path& path,const AssetCancellation& cancel){
    check(cancel);std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Audio source unavailable: "+path.u8string());
    const auto size=file.tellg();if(size<12||size>static_cast<std::streamoff>(sourceBudget))throw std::invalid_argument("Audio source is empty or exceeds 128 MiB");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));file.seekg(0);
    for(std::size_t pos=0;pos<bytes.size();){check(cancel);const auto count=std::min<std::size_t>(65536,bytes.size()-pos);file.read(reinterpret_cast<char*>(bytes.data()+pos),count);if(!file)throw std::runtime_error("Incomplete audio source");pos+=count;}
    if(std::memcmp(bytes.data(),"RIFF",4)==0&&std::memcmp(bytes.data()+8,"WAVE",4)==0){
        const std::uint64_t declared=static_cast<std::uint32_t>(bytes[4])|(static_cast<std::uint32_t>(bytes[5])<<8)|(static_cast<std::uint32_t>(bytes[6])<<16)|(static_cast<std::uint32_t>(bytes[7])<<24);
        if(declared+8>bytes.size())throw std::runtime_error("Truncated WAV container");
    }
    return bytes;
}
class Decoder {
public:
    ma_decoder decoder{};
    Decoder(const std::vector<unsigned char>& bytes,const AudioDecodeSettings& settings){
        if((settings.sampleRate&&(settings.sampleRate<8000||settings.sampleRate>192000))||settings.channels>8||settings.maxDecodedBytes==0||settings.maxDecodedBytes>256ULL*1024*1024)
            throw std::invalid_argument("Invalid audio conversion settings or decoded budget");
        auto config=ma_decoder_config_init(ma_format_f32,settings.channels,settings.sampleRate);
        if(ma_decoder_init_memory(bytes.data(),bytes.size(),&config,&decoder)!=MA_SUCCESS)throw std::runtime_error("Cannot initialize audio decoder");
    }
    ~Decoder(){ma_decoder_uninit(&decoder);}
    AudioDescription description(){
        AudioDescription result;ma_format format{};ma_uint64 frames=0;
        if(ma_decoder_get_data_format(&decoder,&format,&result.channels,&result.sampleRate,nullptr,0)!=MA_SUCCESS||format!=ma_format_f32||
           ma_decoder_get_length_in_pcm_frames(&decoder,&frames)!=MA_SUCCESS||frames==0||result.channels<1||result.channels>8||result.sampleRate<8000||result.sampleRate>192000||frames>static_cast<std::uint64_t>(result.sampleRate)*7200)
            throw std::runtime_error("Invalid audio rate, channels, length or two-hour stream budget");
        result.frames=frames;result.duration=static_cast<double>(frames)/result.sampleRate;result.streaming=result.duration>30;return result;
    }
};
}
AudioDescription probeAudioFile(const std::filesystem::path& path,const AssetCancellation& cancel){
    const auto bytes=read(path,cancel);Decoder decoder(bytes,{});auto result=decoder.description();check(cancel);return result;
}
AudioDescription validateAudioFile(const std::filesystem::path& path,const AssetCancellation& cancel){
    const auto bytes=read(path,cancel);Decoder decoder(bytes,{});const auto result=decoder.description();
    std::vector<float> block(static_cast<std::size_t>(4096)*result.channels);ma_uint64 total=0;
    while(total<result.frames){check(cancel);ma_uint64 got=0;const auto count=std::min<ma_uint64>(4096,result.frames-total);
        const auto status=ma_decoder_read_pcm_frames(&decoder.decoder,block.data(),count,&got);
        if((status!=MA_SUCCESS&&status!=MA_AT_END)||got==0)throw std::runtime_error("Invalid or truncated audio samples");
        for(std::size_t i=0;i<static_cast<std::size_t>(got)*result.channels;++i)if(!std::isfinite(block[i]))throw std::runtime_error("Non-finite audio samples");total+=got;
    }check(cancel);return result;
}
DecodedAudio decodeAudioFile(const std::filesystem::path& path,const AudioDecodeSettings& settings,const AssetCancellation& cancel){
    const auto bytes=read(path,cancel);Decoder decoder(bytes,settings);DecodedAudio result;result.description=decoder.description();
    const auto channels=result.description.channels;const auto frames=result.description.frames;
    if(frames>settings.maxDecodedBytes/(sizeof(float)*channels))throw std::invalid_argument("Decoded audio exceeds its byte budget");
    result.samples.resize(static_cast<std::size_t>(frames)*channels);ma_uint64 total=0;
    while(total<frames){check(cancel);ma_uint64 got=0;const auto count=std::min<ma_uint64>(4096,frames-total);
        const auto status=ma_decoder_read_pcm_frames(&decoder.decoder,result.samples.data()+static_cast<std::size_t>(total)*channels,count,&got);
        if(status!=MA_SUCCESS&&status!=MA_AT_END)throw std::runtime_error("Audio sample decode failed");if(got==0)throw std::runtime_error("Truncated audio samples");total+=got;
    }
    for(float value:result.samples)if(!std::isfinite(value))throw std::runtime_error("Non-finite audio samples");check(cancel);return result;
}
}
