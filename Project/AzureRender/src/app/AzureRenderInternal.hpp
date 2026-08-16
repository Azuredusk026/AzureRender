#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace azurerender::internal {

using Matrix4 = std::array<float, 16>;
using Vector3 = std::array<float, 3>;

inline Matrix4 multiply(const Matrix4& left, const Matrix4& right) {
    Matrix4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[column * 4 + row] +=
                    left[inner * 4 + row] * right[column * 4 + inner];
            }
        }
    }
    return result;
}

inline Vector3 subtract(const Vector3& left, const Vector3& right) {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

inline float dot(const Vector3& left, const Vector3& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

inline Vector3 cross(const Vector3& left, const Vector3& right) {
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
}

inline Vector3 transformPosition(
    const Matrix4& transform,
    const Vector3& position) {
    return {
        transform[0] * position[0] + transform[4] * position[1]
            + transform[8] * position[2] + transform[12],
        transform[1] * position[0] + transform[5] * position[1]
            + transform[9] * position[2] + transform[13],
        transform[2] * position[0] + transform[6] * position[1]
            + transform[10] * position[2] + transform[14],
    };
}

inline Vector3 normalize(const Vector3& value) {
    const float length = std::sqrt(dot(value, value));
    return {value[0] / length, value[1] / length, value[2] / length};
}

inline Matrix4 rotationX(const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, cosine, -sine, 0.0F,
        0.0F, sine, cosine, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

inline Matrix4 rotationY(const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        cosine, 0.0F, -sine, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        sine, 0.0F, cosine, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

inline Matrix4 rotationZ(const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        cosine, -sine, 0.0F, 0.0F,
        sine, cosine, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

inline Matrix4 scale(const float x, const float y, const float z) {
    return {
        x, 0.0F, 0.0F, 0.0F,
        0.0F, y, 0.0F, 0.0F,
        0.0F, 0.0F, z, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

inline Matrix4 translation(const float x, const float y, const float z) {
    return {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        x, y, z, 1.0F,
    };
}

inline Matrix4 uniformScale(const float scale) {
    return {
        scale, 0.0F, 0.0F, 0.0F,
        0.0F, scale, 0.0F, 0.0F,
        0.0F, 0.0F, scale, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

inline Matrix4 lookAt(
    const Vector3& eye,
    const Vector3& target,
    const Vector3& up) {
    const Vector3 forward = normalize(subtract(target, eye));
    const Vector3 right = normalize(cross(forward, up));
    const Vector3 correctedUp = cross(right, forward);
    return {
        right[0], correctedUp[0], -forward[0], 0.0F,
        right[1], correctedUp[1], -forward[1], 0.0F,
        right[2], correctedUp[2], -forward[2], 0.0F,
        -dot(right, eye), -dot(correctedUp, eye), dot(forward, eye), 1.0F,
    };
}

inline Matrix4 perspective(
    const float verticalFov,
    const float aspect,
    const float nearPlane,
    const float farPlane) {
    const float focalLength = 1.0F / std::tan(verticalFov * 0.5F);
    return {
        focalLength / aspect, 0.0F, 0.0F, 0.0F,
        0.0F, -focalLength, 0.0F, 0.0F,
        0.0F, 0.0F, farPlane / (nearPlane - farPlane), -1.0F,
        0.0F, 0.0F, (nearPlane * farPlane) / (nearPlane - farPlane), 0.0F,
    };
}

inline Matrix4 orthographic(
    const float left,
    const float right,
    const float bottom,
    const float top,
    const float nearPlane,
    const float farPlane) {
    return {
        2.0F / (right - left), 0.0F, 0.0F, 0.0F,
        0.0F, -2.0F / (top - bottom), 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F / (nearPlane - farPlane), 0.0F,
        -(right + left) / (right - left),
        (top + bottom) / (top - bottom),
        nearPlane / (nearPlane - farPlane),
        1.0F,
    };
}

// Builds a world-space picking ray direction from a viewport UV coordinate.
// viewportU/v: 0..1 normalized. Uses the same fov/aspect as the renderer.
inline Vector3 pickRayDirection(
    const Vector3& cameraPosition,
    const Vector3& cameraTarget,
    const float viewportU,
    const float viewportV,
    const float aspect) {
    constexpr float kPi = 3.14159265358979323846F;
    const Vector3 forward = normalize(subtract(cameraTarget, cameraPosition));
    const Vector3 worldUp = {0.0F, 1.0F, 0.0F};
    const Vector3 right = normalize(cross(forward, worldUp));
    const Vector3 up = cross(right, forward);
    const float tanHalfFov = std::tan(kPi / 6.0F);
    const float ndcX = viewportU * 2.0F - 1.0F;
    const float ndcY = 1.0F - viewportV * 2.0F;
    return normalize({
        forward[0] + right[0] * ndcX * tanHalfFov * aspect
            + up[0] * ndcY * tanHalfFov,
        forward[1] + right[1] * ndcX * tanHalfFov * aspect
            + up[1] * ndcY * tanHalfFov,
        forward[2] + right[2] * ndcX * tanHalfFov * aspect
            + up[2] * ndcY * tanHalfFov,
    });
}

// Möller–Trumbore ray/triangle intersection. Returns the hit distance along
// the ray, or -1.0 when the ray misses the triangle (including backface
// culling for clockwise winding).
inline float rayTriangleDistance(
    const Vector3& rayOrigin,
    const Vector3& rayDirection,
    const Vector3& v0,
    const Vector3& v1,
    const Vector3& v2) {
    const Vector3 edge1 = subtract(v1, v0);
    const Vector3 edge2 = subtract(v2, v0);
    const Vector3 pvec = cross(rayDirection, edge2);
    const float determinant = dot(edge1, pvec);
    if (std::abs(determinant) < 1.0e-7F) {
        return -1.0F;
    }
    const float inverseDeterminant = 1.0F / determinant;
    const Vector3 tvec = subtract(rayOrigin, v0);
    const float u = dot(tvec, pvec) * inverseDeterminant;
    if (u < 0.0F || u > 1.0F) {
        return -1.0F;
    }
    const Vector3 qvec = cross(tvec, edge1);
    const float v = dot(rayDirection, qvec) * inverseDeterminant;
    if (v < 0.0F || u + v > 1.0F) {
        return -1.0F;
    }
    const float distance = dot(edge2, qvec) * inverseDeterminant;
    return distance > 0.0F ? distance : -1.0F;
}

inline void vkCheck(const VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::string(operation) + " failed with VkResult "
            + std::to_string(result));
    }
}

}  // namespace azurerender::internal
