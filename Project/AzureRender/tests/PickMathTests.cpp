#include "app/AzureRenderInternal.hpp"

#include <cassert>
#include <cmath>

namespace {

using azurerender::internal::pickRayDirection;
using azurerender::internal::rayTriangleDistance;
using azurerender::internal::Vector3;

constexpr float kEpsilon = 1.0e-4F;

bool nearEqual(const float left, const float right) {
    return std::abs(left - right) < kEpsilon;
}

}  // namespace

int main() {
    // Ray at the origin pointing +Z. Triangle lying in the XY plane at z=5.
    const Vector3 origin = {0.0F, 0.0F, 0.0F};
    const Vector3 direction = {0.0F, 0.0F, 1.0F};
    const Vector3 v0 = {-1.0F, -1.0F, 5.0F};
    const Vector3 v1 = {1.0F, -1.0F, 5.0F};
    const Vector3 v2 = {0.0F, 1.0F, 5.0F};

    // Direct hit through the centroid.
    const float hit = rayTriangleDistance(origin, direction, v0, v1, v2);
    assert(hit > 0.0F);
    assert(nearEqual(hit, 5.0F));

    // Ray offset to the side must miss.
    const Vector3 missOrigin = {2.0F, 2.0F, 0.0F};
    assert(rayTriangleDistance(missOrigin, direction, v0, v1, v2) < 0.0F);

    // Ray pointing away must miss.
    const Vector3 awayDirection = {0.0F, 0.0F, -1.0F};
    assert(rayTriangleDistance(origin, awayDirection, v0, v1, v2) < 0.0F);

    // Degenerate triangle (zero area) must miss, not crash.
    const Vector3 degenerate0 = {1.0F, 1.0F, 5.0F};
    const Vector3 degenerate1 = {1.0F, 1.0F, 5.0F};
    const Vector3 degenerate2 = {2.0F, 1.0F, 5.0F};
    assert(rayTriangleDistance(
        origin, direction, degenerate0, degenerate1, degenerate2) < 0.0F);

    // pickRayDirection: center of viewport looks straight ahead (+Z).
    const Vector3 camera = {0.0F, 0.0F, 0.0F};
    const Vector3 target = {0.0F, 0.0F, 1.0F};
    const Vector3 centerDirection = pickRayDirection(camera, target, 0.5F, 0.5F, 1.0F);
    assert(nearEqual(centerDirection[0], 0.0F));
    assert(nearEqual(centerDirection[1], 0.0F));
    assert(centerDirection[2] > 0.9F);

    // Corner of viewport deviates from the forward axis.
    const Vector3 cornerDirection = pickRayDirection(camera, target, 0.0F, 0.0F, 1.0F);
    assert(cornerDirection[0] > 0.0F);
    assert(cornerDirection[1] > 0.0F);

    // Wide aspect bends the horizontal axis further than vertical.
    const Vector3 wideDirection = pickRayDirection(camera, target, 0.0F, 0.5F, 2.0F);
    const Vector3 narrowDirection = pickRayDirection(camera, target, 0.0F, 0.5F, 1.0F);
    assert(std::abs(wideDirection[0]) > std::abs(narrowDirection[0]));

    // End-to-end: ray through a triangle positioned along the camera forward.
    const Vector3 tri0 = {0.0F, 1.0F, 4.0F};
    const Vector3 tri1 = {-1.0F, -1.0F, 4.0F};
    const Vector3 tri2 = {1.0F, -1.0F, 4.0F};
    const float centerHit = rayTriangleDistance(
        camera, centerDirection, tri0, tri1, tri2);
    assert(centerHit > 0.0F);
    assert(nearEqual(centerHit, 4.0F));

    return 0;
}