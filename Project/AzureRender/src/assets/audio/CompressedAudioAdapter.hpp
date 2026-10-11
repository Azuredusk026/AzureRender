#pragma once
#include "assets/audio/AudioCodec.hpp"
namespace azurerender {
// Returns no reader for containers handled by the ordinary audio decoder.
std::unique_ptr<AudioFrameReader> openCompressedAudioReader(const std::filesystem::path& path,const AssetCancellation& cancelled);
}
