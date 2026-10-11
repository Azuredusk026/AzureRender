#pragma once
#include "assets/AssetFormatRegistry.hpp"
#include <cstdint>
#include <memory>
namespace azurerender {
struct AudioDescription {
    std::uint32_t sampleRate=0,channels=0;
    std::uint64_t frames=0;
    double duration=0;
    bool streaming=false;
};
struct AudioDecodeSettings {
    std::uint32_t sampleRate=0,channels=0;
    std::uint64_t maxDecodedBytes=256ULL*1024*1024;
};
struct DecodedAudio {AudioDescription description;std::vector<float> samples;};
// Readers own decoder state. Callers serialize reads and seeks on each reader.
// Each read uses native-rate interleaved Float32 samples and bounded blocks.
class AudioFrameReader {
public:
    virtual ~AudioFrameReader()=default;
    virtual AudioDescription description() const=0;
    virtual std::uint64_t read(float* samples,std::uint64_t frames,const AssetCancellation& cancelled={})=0;
    virtual void seek(std::uint64_t frame,const AssetCancellation& cancelled={})=0;
};
std::unique_ptr<AudioFrameReader> openAudioFileReader(const std::filesystem::path& path,const AssetCancellation& cancelled={});
// RenderCore owns codecs; Runtime owns device, mixer and sound lifetime.
// Samples are interleaved Float32. Zero settings retain the source format.
AudioDescription probeAudioFile(const std::filesystem::path& path,const AssetCancellation& cancelled={});
AudioDescription validateAudioFile(const std::filesystem::path& path,const AssetCancellation& cancelled={});
DecodedAudio decodeAudioFile(const std::filesystem::path& path,const AudioDecodeSettings& settings={},const AssetCancellation& cancelled={});
void registerAudioFormats(AssetFormatRegistry& registry);
}
