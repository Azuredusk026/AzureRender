#pragma once
#include "assets/AssetFormatRegistry.hpp"
#include <cstdint>
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
// RenderCore owns codecs; Runtime owns device, mixer and sound lifetime.
// Samples are interleaved Float32. Zero settings retain the source format.
AudioDescription probeAudioFile(const std::filesystem::path& path,const AssetCancellation& cancelled={});
AudioDescription validateAudioFile(const std::filesystem::path& path,const AssetCancellation& cancelled={});
DecodedAudio decodeAudioFile(const std::filesystem::path& path,const AudioDecodeSettings& settings={},const AssetCancellation& cancelled={});
void registerAudioFormats(AssetFormatRegistry& registry);
}
