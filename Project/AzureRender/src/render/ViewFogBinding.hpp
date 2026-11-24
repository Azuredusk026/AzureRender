#pragma once
#include "render/FogViewProjection.hpp"
#include "render/CameraViewDescriptor.hpp"
#include "render/AnalyticFog.hpp"
#include <optional>
#include <string>
#include <string_view>
namespace azurerender {
enum class FogViewMatch { Disabled, Matched, CameraMismatch, AspectMismatch, AnalyticOnly, GeneratedHeight };
struct GeneratedHeightFogSettings {
    static constexpr std::array<unsigned,3> maximumDimensions{64,64,128};
    static constexpr float maximumViewDepth=100000.F;
    std::array<unsigned,3> dimensions{};
    float farViewDepth=0;

    static bool validDimensions(const std::array<unsigned,3>& extent) noexcept {
        return extent[0]>0&&extent[0]<=maximumDimensions[0]
            &&extent[1]>0&&extent[1]<=maximumDimensions[1]
            &&extent[2]>=2&&extent[2]<=maximumDimensions[2];
    }
};
struct ViewFogBinding {
    bool enabled=false;
    std::string asset;
    FogViewProjection projection;
    std::optional<CameraViewDescriptor> captureView;
    std::optional<AnalyticFogSettings> analytic;
    std::optional<GeneratedHeightFogSettings> generatedHeightVolume;
    void validate() const;
    FogViewMatch match(const CameraViewDescriptor& current,float viewportAspect) const;
};
std::string_view fogViewMatchName(FogViewMatch) noexcept;
}
