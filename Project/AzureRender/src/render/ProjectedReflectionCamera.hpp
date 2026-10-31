#pragma once
#include "render/ProjectedReflectionRay.hpp"
#include "render/ReflectionRay.hpp"
namespace azurerender {
// Converts a forward-device-depth perspective view to reverse depth and upper-left UV.
// Positions and orthonormal axes must describe the corresponding sampled frame.
ProjectedReflectionView projectedReflectionView(const ReflectionTraceView& current,const ReflectionTraceView* previous=nullptr);
}
