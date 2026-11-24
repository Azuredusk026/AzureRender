#pragma once
#include "render/FogViewProjection.hpp"
#include "render/CameraViewDescriptor.hpp"
#include "render/AnalyticFog.hpp"
#include <optional>
#include <string>
#include <string_view>
namespace azurerender {
enum class FogViewMatch { Disabled, Matched, CameraMismatch, AspectMismatch, AnalyticOnly };
struct ViewFogBinding {
    bool enabled=false;
    std::string asset;
    FogViewProjection projection;
    std::optional<CameraViewDescriptor> captureView;
    std::optional<AnalyticFogSettings> analytic;
    void validate() const;
    FogViewMatch match(const CameraViewDescriptor& current,float viewportAspect) const;
};
std::string_view fogViewMatchName(FogViewMatch) noexcept;
}
