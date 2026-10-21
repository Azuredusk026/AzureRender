#pragma once
#include <cstdint>
namespace azurerender {
struct AnimationClipSample {
    std::uint32_t clip=0;
    double time=0;
    float weight=1;
    bool loop=true;
    double duration=0; // Positive values require an exact bound clip duration.
};
}
