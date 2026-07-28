#include "GltfLoader.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using Matrix4 = std::array<double, 16>;

Matrix4 identityMatrix() {
    return {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };
}

Matrix4 multiply(const Matrix4& left, const Matrix4& right) {
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

Matrix4 nodeTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        Matrix4 result{};
        std::copy(node.matrix.begin(), node.matrix.end(), result.begin());
        return result;
    }

    const double translationX = node.translation.size() == 3 ? node.translation[0] : 0.0;
    const double translationY = node.translation.size() == 3 ? node.translation[1] : 0.0;
    const double translationZ = node.translation.size() == 3 ? node.translation[2] : 0.0;
    const double scaleX = node.scale.size() == 3 ? node.scale[0] : 1.0;
    const double scaleY = node.scale.size() == 3 ? node.scale[1] : 1.0;
    const double scaleZ = node.scale.size() == 3 ? node.scale[2] : 1.0;
    const double x = node.rotation.size() == 4 ? node.rotation[0] : 0.0;
    const double y = node.rotation.size() == 4 ? node.rotation[1] : 0.0;
    const double z = node.rotation.size() == 4 ? node.rotation[2] : 0.0;
    const double w = node.rotation.size() == 4 ? node.rotation[3] : 1.0;

    Matrix4 result = {
        (1.0 - 2.0 * y * y - 2.0 * z * z) * scaleX,
        (2.0 * x * y + 2.0 * w * z) * scaleX,
        (2.0 * x * z - 2.0 * w * y) * scaleX,
        0.0,
        (2.0 * x * y - 2.0 * w * z) * scaleY,
        (1.0 - 2.0 * x * x - 2.0 * z * z) * scaleY,
        (2.0 * y * z + 2.0 * w * x) * scaleY,
        0.0,
        (2.0 * x * z + 2.0 * w * y) * scaleZ,
        (2.0 * y * z - 2.0 * w * x) * scaleZ,
        (1.0 - 2.0 * x * x - 2.0 * y * y) * scaleZ,
        0.0,
        translationX, translationY, translationZ, 1.0,
    };
    return result;
}

void transformVertex(AssetVertex& vertex, const Matrix4& transform) {
    const auto position = vertex.position;
    vertex.position = {
        static_cast<float>(
            transform[0] * position[0] + transform[4] * position[1]
            + transform[8] * position[2] + transform[12]),
        static_cast<float>(
            transform[1] * position[0] + transform[5] * position[1]
            + transform[9] * position[2] + transform[13]),
        static_cast<float>(
            transform[2] * position[0] + transform[6] * position[1]
            + transform[10] * position[2] + transform[14]),
    };

    const double a00 = transform[0];
    const double a01 = transform[4];
    const double a02 = transform[8];
    const double a10 = transform[1];
    const double a11 = transform[5];
    const double a12 = transform[9];
    const double a20 = transform[2];
    const double a21 = transform[6];
    const double a22 = transform[10];
    const double determinant =
        a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);
    if (std::abs(determinant) < 1.0e-10) {
        throw std::runtime_error("glTF node has a non-invertible transform");
    }

    const auto normal = vertex.normal;
    const double nx =
        ((a11 * a22 - a12 * a21) * normal[0]
         + (a12 * a20 - a10 * a22) * normal[1]
         + (a10 * a21 - a11 * a20) * normal[2]) / determinant;
    const double ny =
        ((a02 * a21 - a01 * a22) * normal[0]
         + (a00 * a22 - a02 * a20) * normal[1]
         + (a01 * a20 - a00 * a21) * normal[2]) / determinant;
    const double nz =
        ((a01 * a12 - a02 * a11) * normal[0]
         + (a02 * a10 - a00 * a12) * normal[1]
         + (a00 * a11 - a01 * a10) * normal[2]) / determinant;
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    vertex.normal = {
        static_cast<float>(nx / length),
        static_cast<float>(ny / length),
        static_cast<float>(nz / length),
    };

    const auto tangent = vertex.tangent;
    double tx =
        a00 * tangent[0] + a01 * tangent[1] + a02 * tangent[2];
    double ty =
        a10 * tangent[0] + a11 * tangent[1] + a12 * tangent[2];
    double tz =
        a20 * tangent[0] + a21 * tangent[1] + a22 * tangent[2];
    const double tangentDotNormal =
        tx * vertex.normal[0] + ty * vertex.normal[1] + tz * vertex.normal[2];
    tx -= tangentDotNormal * vertex.normal[0];
    ty -= tangentDotNormal * vertex.normal[1];
    tz -= tangentDotNormal * vertex.normal[2];
    const double tangentLength = std::sqrt(tx * tx + ty * ty + tz * tz);
    vertex.tangent = {
        static_cast<float>(tx / tangentLength),
        static_cast<float>(ty / tangentLength),
        static_cast<float>(tz / tangentLength),
        tangent[3] * (determinant < 0.0 ? -1.0F : 1.0F),
    };
}

const unsigned char* accessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor) {
    const auto& view = model.bufferViews.at(static_cast<std::size_t>(accessor.bufferView));
    const auto& buffer = model.buffers.at(static_cast<std::size_t>(view.buffer));
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

std::size_t accessorStride(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor) {
    const auto& view = model.bufferViews.at(static_cast<std::size_t>(accessor.bufferView));
    const int stride = accessor.ByteStride(view);
    if (stride <= 0) {
        throw std::runtime_error("glTF accessor has an invalid byte stride");
    }
    return static_cast<std::size_t>(stride);
}

void copyFloatAttribute(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const char* name,
    const std::size_t componentCount,
    std::vector<AssetVertex>& vertices) {
    const auto attribute = primitive.attributes.find(name);
    if (attribute == primitive.attributes.end()) {
        return;
    }
    const auto& accessor = model.accessors.at(static_cast<std::size_t>(attribute->second));
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT
        || accessor.count != vertices.size()) {
        throw std::runtime_error(std::string("Unsupported glTF attribute layout: ") + name);
    }

    const unsigned char* source = accessorData(model, accessor);
    const std::size_t stride = accessorStride(model, accessor);
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        std::array<float, 4> values{};
        std::memcpy(values.data(), source + index * stride, componentCount * sizeof(float));
        if (std::strcmp(name, "POSITION") == 0) {
            std::copy_n(values.begin(), componentCount, vertices[index].position.begin());
        } else if (std::strcmp(name, "NORMAL") == 0) {
            std::copy_n(values.begin(), componentCount, vertices[index].normal.begin());
        } else if (std::strcmp(name, "TANGENT") == 0) {
            std::copy_n(values.begin(), componentCount, vertices[index].tangent.begin());
        } else {
            std::copy_n(values.begin(), componentCount, vertices[index].texcoord.begin());
        }
    }
}

std::uint32_t readUnsignedComponent(
    const unsigned char* source,
    const int componentType) {
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return *source;
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        std::uint16_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::uint32_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
    throw std::runtime_error("Unsupported JOINTS_0 component type");
}

float readWeightComponent(
    const unsigned char* source,
    const int componentType,
    const bool normalized) {
    if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        float value = 0.0F;
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
    if (!normalized) {
        throw std::runtime_error(
            "Integer WEIGHTS_0 accessor must be normalized");
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return static_cast<float>(*source) / 255.0F;
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        std::uint16_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return static_cast<float>(value) / 65535.0F;
    }
    throw std::runtime_error("Unsupported WEIGHTS_0 component type");
}

std::size_t componentByteSize(const int componentType) {
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return sizeof(std::uint8_t);
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return sizeof(std::uint16_t);
    }
    if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
        || componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        return sizeof(std::uint32_t);
    }
    throw std::runtime_error("Unsupported glTF skin component type");
}

void copySkinAttributes(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    std::vector<AssetVertex>& vertices) {
    const auto jointsAttribute = primitive.attributes.find("JOINTS_0");
    const auto weightsAttribute = primitive.attributes.find("WEIGHTS_0");
    if (jointsAttribute == primitive.attributes.end()
        && weightsAttribute == primitive.attributes.end()) {
        return;
    }
    if (jointsAttribute == primitive.attributes.end()
        || weightsAttribute == primitive.attributes.end()) {
        throw std::runtime_error(
            "Skinned glTF primitive must provide both JOINTS_0 and WEIGHTS_0");
    }

    const auto& jointsAccessor =
        model.accessors.at(static_cast<std::size_t>(jointsAttribute->second));
    const auto& weightsAccessor =
        model.accessors.at(static_cast<std::size_t>(weightsAttribute->second));
    if (jointsAccessor.type != TINYGLTF_TYPE_VEC4
        || weightsAccessor.type != TINYGLTF_TYPE_VEC4
        || jointsAccessor.count != vertices.size()
        || weightsAccessor.count != vertices.size()) {
        throw std::runtime_error("Unsupported glTF skin attribute layout");
    }

    const unsigned char* jointsSource = accessorData(model, jointsAccessor);
    const unsigned char* weightsSource = accessorData(model, weightsAccessor);
    const std::size_t jointsStride = accessorStride(model, jointsAccessor);
    const std::size_t weightsStride = accessorStride(model, weightsAccessor);
    const std::size_t jointComponentSize =
        componentByteSize(jointsAccessor.componentType);
    const std::size_t weightComponentSize =
        componentByteSize(weightsAccessor.componentType);
    for (std::size_t vertexIndex = 0;
         vertexIndex < vertices.size();
         ++vertexIndex) {
        float weightSum = 0.0F;
        for (std::size_t component = 0; component < 4; ++component) {
            vertices[vertexIndex].joints[component] =
                readUnsignedComponent(
                    jointsSource + vertexIndex * jointsStride
                        + component * jointComponentSize,
                    jointsAccessor.componentType);
            const float weight = readWeightComponent(
                weightsSource + vertexIndex * weightsStride
                    + component * weightComponentSize,
                weightsAccessor.componentType,
                weightsAccessor.normalized);
            vertices[vertexIndex].weights[component] = weight;
            weightSum += weight;
        }
        if (weightSum <= 1.0e-8F) {
            vertices[vertexIndex].joints = {};
            vertices[vertexIndex].weights = {1.0F, 0.0F, 0.0F, 0.0F};
            continue;
        }
        for (float& weight : vertices[vertexIndex].weights) {
            weight /= weightSum;
        }
    }
}

void generateTangents(
    std::vector<AssetVertex>& vertices,
    const std::vector<std::uint32_t>& indices) {
    std::vector<std::array<float, 3>> tangentSums(vertices.size());
    std::vector<std::array<float, 3>> bitangentSums(vertices.size());
    for (std::size_t triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        const std::uint32_t i0 = indices[triangle];
        const std::uint32_t i1 = indices[triangle + 1];
        const std::uint32_t i2 = indices[triangle + 2];
        const auto& p0 = vertices[i0].position;
        const auto& p1 = vertices[i1].position;
        const auto& p2 = vertices[i2].position;
        const auto& uv0 = vertices[i0].texcoord;
        const auto& uv1 = vertices[i1].texcoord;
        const auto& uv2 = vertices[i2].texcoord;
        const float edge1X = p1[0] - p0[0];
        const float edge1Y = p1[1] - p0[1];
        const float edge1Z = p1[2] - p0[2];
        const float edge2X = p2[0] - p0[0];
        const float edge2Y = p2[1] - p0[1];
        const float edge2Z = p2[2] - p0[2];
        const float deltaU1 = uv1[0] - uv0[0];
        const float deltaV1 = uv1[1] - uv0[1];
        const float deltaU2 = uv2[0] - uv0[0];
        const float deltaV2 = uv2[1] - uv0[1];
        const float denominator = deltaU1 * deltaV2 - deltaU2 * deltaV1;
        if (std::abs(denominator) < 1.0e-8F) {
            continue;
        }
        const float reciprocal = 1.0F / denominator;
        const std::array<float, 3> tangent = {
            (edge1X * deltaV2 - edge2X * deltaV1) * reciprocal,
            (edge1Y * deltaV2 - edge2Y * deltaV1) * reciprocal,
            (edge1Z * deltaV2 - edge2Z * deltaV1) * reciprocal,
        };
        const std::array<float, 3> bitangent = {
            (edge2X * deltaU1 - edge1X * deltaU2) * reciprocal,
            (edge2Y * deltaU1 - edge1Y * deltaU2) * reciprocal,
            (edge2Z * deltaU1 - edge1Z * deltaU2) * reciprocal,
        };
        for (const std::uint32_t vertexIndex : {i0, i1, i2}) {
            for (std::size_t component = 0; component < 3; ++component) {
                tangentSums[vertexIndex][component] += tangent[component];
                bitangentSums[vertexIndex][component] += bitangent[component];
            }
        }
    }

    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const auto& normal = vertices[index].normal;
        auto tangent = tangentSums[index];
        const float projection =
            tangent[0] * normal[0] + tangent[1] * normal[1] + tangent[2] * normal[2];
        for (std::size_t component = 0; component < 3; ++component) {
            tangent[component] -= projection * normal[component];
        }
        const float length = std::sqrt(
            tangent[0] * tangent[0] + tangent[1] * tangent[1]
            + tangent[2] * tangent[2]);
        if (length < 1.0e-8F) {
            vertices[index].tangent = {1.0F, 0.0F, 0.0F, 1.0F};
            continue;
        }
        for (float& component : tangent) {
            component /= length;
        }
        const std::array<float, 3> crossNormalTangent = {
            normal[1] * tangent[2] - normal[2] * tangent[1],
            normal[2] * tangent[0] - normal[0] * tangent[2],
            normal[0] * tangent[1] - normal[1] * tangent[0],
        };
        const auto& bitangent = bitangentSums[index];
        const float handedness =
            crossNormalTangent[0] * bitangent[0]
                + crossNormalTangent[1] * bitangent[1]
                + crossNormalTangent[2] * bitangent[2] < 0.0F
            ? -1.0F
            : 1.0F;
        vertices[index].tangent = {
            tangent[0], tangent[1], tangent[2], handedness};
    }
}

std::vector<std::uint32_t> readIndices(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const std::size_t vertexCount) {
    if (primitive.indices < 0) {
        std::vector<std::uint32_t> sequential(vertexCount);
        for (std::size_t index = 0; index < vertexCount; ++index) {
            sequential[index] = static_cast<std::uint32_t>(index);
        }
        return sequential;
    }

    const auto& accessor = model.accessors.at(static_cast<std::size_t>(primitive.indices));
    const unsigned char* source = accessorData(model, accessor);
    const std::size_t stride = accessorStride(model, accessor);
    std::vector<std::uint32_t> result(accessor.count);
    for (std::size_t index = 0; index < accessor.count; ++index) {
        const unsigned char* element = source + index * stride;
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            result[index] = *element;
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            std::uint16_t value = 0;
            std::memcpy(&value, element, sizeof(value));
            result[index] = value;
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            std::memcpy(&result[index], element, sizeof(result[index]));
        } else {
            throw std::runtime_error("Unsupported glTF index component type");
        }
    }
    return result;
}

std::vector<std::uint8_t> decodeImageRgba(
    const tinygltf::Model& model,
    const int imageIndex,
    const std::array<double, 4>& factor,
    std::uint32_t& width,
    std::uint32_t& height) {
    const auto& image = model.images.at(static_cast<std::size_t>(imageIndex));
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
        throw std::runtime_error("glTF material image did not decode");
    }
    width = static_cast<std::uint32_t>(image.width);
    height = static_cast<std::uint32_t>(image.height);
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * height * 4);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height;
         ++pixel) {
        const std::size_t source = pixel * static_cast<std::size_t>(image.component);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::uint8_t sourceValue = channel < static_cast<std::size_t>(image.component)
                ? image.image[source + channel]
                : (channel == 3 ? 255 : image.image[source]);
            pixels[pixel * 4 + channel] = static_cast<std::uint8_t>(
                static_cast<double>(sourceValue) * std::clamp(factor[channel], 0.0, 1.0));
        }
    }
    return pixels;
}

std::array<float, 4> readMaterialColorExtra(
    const tinygltf::Value& extras,
    const std::string& key) {
    std::array<float, 4> result{1.0F, 1.0F, 1.0F, 0.0F};
    if (!extras.IsObject() || !extras.Has(key)) {
        return result;
    }
    const tinygltf::Value& value = extras.Get(key);
    if (!value.IsArray() || value.ArrayLen() < 3) {
        return result;
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const tinygltf::Value& component = value.Get(channel);
        if (!component.IsNumber()) {
            return {1.0F, 1.0F, 1.0F, 0.0F};
        }
        result[channel] = static_cast<float>(
            std::clamp(component.GetNumberAsDouble(), 0.0, 1.0));
    }
    result[3] = 1.0F;
    return result;
}

std::array<float, 4> readMaterialVectorExtra(
    const tinygltf::Value& extras,
    const std::string& key,
    const std::array<float, 4>& fallback) {
    if (!extras.IsObject() || !extras.Has(key)) {
        return fallback;
    }
    const tinygltf::Value& value = extras.Get(key);
    if (!value.IsArray() || value.ArrayLen() < 4) {
        return fallback;
    }
    std::array<float, 4> result{};
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const tinygltf::Value& component = value.Get(channel);
        if (!component.IsNumber()) {
            return fallback;
        }
        result[channel] =
            static_cast<float>(component.GetNumberAsDouble());
    }
    return result;
}

AssetMaterial loadMaterial(
    const tinygltf::Model& model,
    const tinygltf::Material* material) {
    AssetMaterial result;
    int baseColorImage = -1;
    int normalImage = -1;
    int metallicRoughnessImage = -1;
    int specularEmissiveImage = -1;
    int styleMaskImage = -1;
    int matcapImage = -1;
    int hairDataImage = -1;
    std::array<double, 4> factor{1.0, 1.0, 1.0, 1.0};
    if (material != nullptr) {
        const auto& sourceFactor = material->pbrMetallicRoughness.baseColorFactor;
        if (sourceFactor.size() == 4) {
            std::copy(sourceFactor.begin(), sourceFactor.end(), factor.begin());
        }
        const int baseColorTexture = material->pbrMetallicRoughness.baseColorTexture.index;
        if (baseColorTexture >= 0) {
            baseColorImage =
                model.textures.at(static_cast<std::size_t>(baseColorTexture)).source;
        }
        const int normalTexture = material->normalTexture.index;
        if (normalTexture >= 0) {
            normalImage = model.textures.at(static_cast<std::size_t>(normalTexture)).source;
        }
        const int metallicRoughnessTexture =
            material->pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (metallicRoughnessTexture >= 0) {
            metallicRoughnessImage = model.textures.at(
                static_cast<std::size_t>(metallicRoughnessTexture)).source;
        }
        const int emissiveTexture = material->emissiveTexture.index;
        if (emissiveTexture >= 0) {
            specularEmissiveImage =
                model.textures.at(static_cast<std::size_t>(emissiveTexture)).source;
        }
        const int styleMaskTexture = material->occlusionTexture.index;
        if (styleMaskTexture >= 0) {
            styleMaskImage =
                model.textures.at(static_cast<std::size_t>(styleMaskTexture)).source;
        }
        if (material->emissiveFactor.size() == 3) {
            result.emissiveStrength = static_cast<float>(std::max({
                material->emissiveFactor[0],
                material->emissiveFactor[1],
                material->emissiveFactor[2],
            }));
        }
        result.aoColor =
            readMaterialColorExtra(material->extras, "afterglowAoColor");
        result.lamShadowColor =
            readMaterialColorExtra(material->extras, "afterglowLamShadowColor");
        result.matcapColor =
            readMaterialColorExtra(material->extras, "afterglowMatcapColor");
        result.hairParameters = readMaterialVectorExtra(
            material->extras,
            "afterglowHairParameters",
            result.hairParameters);
        if (
            material->extras.IsObject()
            && material->extras.Has("afterglowMatcapTexture")) {
            const tinygltf::Value& matcapTexture =
                material->extras.Get("afterglowMatcapTexture");
            if (matcapTexture.IsNumber()) {
                const int textureIndex =
                    static_cast<int>(matcapTexture.GetNumberAsDouble());
                if (
                    textureIndex >= 0
                    && static_cast<std::size_t>(textureIndex)
                        < model.textures.size()) {
                    matcapImage =
                        model.textures[static_cast<std::size_t>(textureIndex)].source;
                }
            }
        }
        if (
            material->extras.IsObject()
            && material->extras.Has("afterglowHairDataTexture")) {
            const tinygltf::Value& hairDataTexture =
                material->extras.Get("afterglowHairDataTexture");
            if (hairDataTexture.IsNumber()) {
                const int textureIndex =
                    static_cast<int>(hairDataTexture.GetNumberAsDouble());
                if (
                    textureIndex >= 0
                    && static_cast<std::size_t>(textureIndex)
                        < model.textures.size()) {
                    hairDataImage =
                        model.textures[static_cast<std::size_t>(textureIndex)].source;
                }
            }
        }
        result.alphaCutoff = static_cast<float>(material->alphaCutoff);
        result.doubleSided = material->doubleSided;
        if (material->alphaMode == "MASK") {
            result.alphaMode = AssetAlphaMode::Mask;
        } else if (material->alphaMode == "BLEND") {
            result.alphaMode = AssetAlphaMode::Blend;
        }
    }

    if (baseColorImage >= 0) {
        result.baseColorPixels = decodeImageRgba(
            model,
            baseColorImage,
            factor,
            result.baseColorWidth,
            result.baseColorHeight);
    } else {
        result.baseColorWidth = 2;
        result.baseColorHeight = 2;
        result.baseColorPixels.resize(16);
        for (std::size_t pixel = 0; pixel < 4; ++pixel) {
            for (std::size_t channel = 0; channel < 4; ++channel) {
                result.baseColorPixels[pixel * 4 + channel] =
                    static_cast<std::uint8_t>(
                        std::clamp(factor[channel], 0.0, 1.0) * 255.0);
            }
        }
    }

    if (normalImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.normalPixels = decodeImageRgba(
            model,
            normalImage,
            kNoFactor,
            result.normalWidth,
            result.normalHeight);
    } else {
        result.normalWidth = 2;
        result.normalHeight = 2;
        result.normalPixels = {
            128, 128, 255, 255, 128, 128, 255, 255,
            128, 128, 255, 255, 128, 128, 255, 255,
        };
    }
    if (metallicRoughnessImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.metallicRoughnessPixels = decodeImageRgba(
            model,
            metallicRoughnessImage,
            kNoFactor,
            result.metallicRoughnessWidth,
            result.metallicRoughnessHeight);
    } else {
        result.metallicRoughnessWidth = 2;
        result.metallicRoughnessHeight = 2;
        result.metallicRoughnessPixels = {
            255, 191, 0, 255, 255, 191, 0, 255,
            255, 191, 0, 255, 255, 191, 0, 255,
        };
    }
    if (specularEmissiveImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.specularEmissivePixels = decodeImageRgba(
            model,
            specularEmissiveImage,
            kNoFactor,
            result.specularEmissiveWidth,
            result.specularEmissiveHeight);
    } else {
        result.specularEmissiveWidth = 2;
        result.specularEmissiveHeight = 2;
        result.specularEmissivePixels = {
            0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255,
        };
    }
    if (styleMaskImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.styleMaskPixels = decodeImageRgba(
            model,
            styleMaskImage,
            kNoFactor,
            result.styleMaskWidth,
            result.styleMaskHeight);
    } else {
        result.styleMaskWidth = 2;
        result.styleMaskHeight = 2;
        result.styleMaskPixels = {
            0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255,
        };
    }
    if (matcapImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.matcapPixels = decodeImageRgba(
            model,
            matcapImage,
            kNoFactor,
            result.matcapWidth,
            result.matcapHeight);
    } else {
        result.matcapWidth = 2;
        result.matcapHeight = 2;
        result.matcapPixels = {
            0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255,
        };
    }
    if (hairDataImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.hairDataPixels = decodeImageRgba(
            model,
            hairDataImage,
            kNoFactor,
            result.hairDataWidth,
            result.hairDataHeight);
    } else {
        result.hairDataWidth = 2;
        result.hairDataHeight = 2;
        result.hairDataPixels = {
            128, 128, 128, 128, 128, 128, 128, 128,
            128, 128, 128, 128, 128, 128, 128, 128,
        };
    }
    return result;
}

std::vector<Matrix4> calculateNodeWorldTransforms(
    const tinygltf::Model& model) {
    std::vector<int> parents(model.nodes.size(), -1);
    for (std::size_t parent = 0; parent < model.nodes.size(); ++parent) {
        for (const int child : model.nodes[parent].children) {
            parents.at(static_cast<std::size_t>(child)) =
                static_cast<int>(parent);
        }
    }

    std::vector<Matrix4> worldTransforms(
        model.nodes.size(),
        identityMatrix());
    std::vector<bool> calculated(model.nodes.size(), false);
    std::function<const Matrix4&(std::size_t)> calculate =
        [&](const std::size_t index) -> const Matrix4& {
        if (calculated[index]) {
            return worldTransforms[index];
        }
        const Matrix4 local = nodeTransform(model.nodes[index]);
        const int parent = parents[index];
        worldTransforms[index] = parent >= 0
            ? multiply(calculate(static_cast<std::size_t>(parent)), local)
            : local;
        calculated[index] = true;
        return worldTransforms[index];
    };
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        calculate(index);
    }
    return worldTransforms;
}

void loadJointMatrices(
    const tinygltf::Model& model,
    const std::vector<Matrix4>& nodeWorldTransforms,
    LoadedAsset& asset) {
    if (model.skins.empty()) {
        std::array<float, 16> identity{};
        const Matrix4 sourceIdentity = identityMatrix();
        std::transform(
            sourceIdentity.begin(),
            sourceIdentity.end(),
            identity.begin(),
            [](const double value) { return static_cast<float>(value); });
        asset.jointMatrices.push_back(identity);
        return;
    }
    if (model.skins.size() != 1) {
        throw std::runtime_error(
            "The current renderer supports one glTF skin per asset");
    }

    const tinygltf::Skin& skin = model.skins.front();
    if (skin.joints.empty()) {
        throw std::runtime_error("glTF skin contains no joints");
    }
    std::vector<Matrix4> inverseBindMatrices(
        skin.joints.size(),
        identityMatrix());
    if (skin.inverseBindMatrices >= 0) {
        const auto& accessor = model.accessors.at(
            static_cast<std::size_t>(skin.inverseBindMatrices));
        if (accessor.type != TINYGLTF_TYPE_MAT4
            || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT
            || accessor.count != skin.joints.size()) {
            throw std::runtime_error(
                "Unsupported inverse bind matrix accessor layout");
        }
        const unsigned char* source = accessorData(model, accessor);
        const std::size_t stride = accessorStride(model, accessor);
        for (std::size_t index = 0; index < skin.joints.size(); ++index) {
            std::array<float, 16> values{};
            std::memcpy(
                values.data(),
                source + index * stride,
                sizeof(values));
            std::transform(
                values.begin(),
                values.end(),
                inverseBindMatrices[index].begin(),
                [](const float value) { return static_cast<double>(value); });
        }
    }

    asset.jointMatrices.reserve(skin.joints.size());
    for (std::size_t index = 0; index < skin.joints.size(); ++index) {
        const int jointNode = skin.joints[index];
        const Matrix4 jointMatrix = multiply(
            nodeWorldTransforms.at(static_cast<std::size_t>(jointNode)),
            inverseBindMatrices[index]);
        std::array<float, 16> gpuMatrix{};
        std::transform(
            jointMatrix.begin(),
            jointMatrix.end(),
            gpuMatrix.begin(),
            [](const double value) { return static_cast<float>(value); });
        asset.jointMatrices.push_back(gpuMatrix);
    }
    asset.hasSkin = true;
}

void appendPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const Matrix4& transform,
    const bool skinned,
    const std::uint32_t fallbackMaterial,
    LoadedAsset& asset) {
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
        throw std::runtime_error("Only triangle-list glTF primitives are supported");
    }
    const auto position = primitive.attributes.find("POSITION");
    if (position == primitive.attributes.end()) {
        throw std::runtime_error("glTF primitive has no POSITION attribute");
    }

    const auto& positionAccessor =
        model.accessors.at(static_cast<std::size_t>(position->second));
    std::vector<AssetVertex> vertices(positionAccessor.count);
    copyFloatAttribute(model, primitive, "POSITION", 3, vertices);
    copyFloatAttribute(model, primitive, "NORMAL", 3, vertices);
    copyFloatAttribute(model, primitive, "TANGENT", 4, vertices);
    copyFloatAttribute(model, primitive, "TEXCOORD_0", 2, vertices);
    copySkinAttributes(model, primitive, vertices);
    std::vector<std::uint32_t> indices = readIndices(model, primitive, vertices.size());
    if (primitive.attributes.find("TANGENT") == primitive.attributes.end()) {
        generateTangents(vertices, indices);
    }
    if (!skinned) {
        for (auto& vertex : vertices) {
            transformVertex(vertex, transform);
        }
    } else {
        for (const AssetVertex& vertex : vertices) {
            for (const std::uint32_t joint : vertex.joints) {
                if (joint >= asset.jointMatrices.size()) {
                    throw std::runtime_error(
                        "JOINTS_0 index exceeds the glTF skin joint count");
                }
            }
        }
    }

    const std::uint32_t baseVertex = static_cast<std::uint32_t>(asset.vertices.size());
    for (auto& index : indices) {
        index += baseVertex;
    }

    AssetPrimitive draw{};
    draw.firstIndex = static_cast<std::uint32_t>(asset.indices.size());
    draw.indexCount = static_cast<std::uint32_t>(indices.size());
    draw.materialIndex = primitive.material >= 0
        ? static_cast<std::uint32_t>(primitive.material)
        : fallbackMaterial;
    std::array<float, 3> primitiveMin{};
    std::array<float, 3> primitiveMax{};
    primitiveMin.fill(std::numeric_limits<float>::max());
    primitiveMax.fill(std::numeric_limits<float>::lowest());
    for (const AssetVertex& vertex : vertices) {
        for (std::size_t component = 0; component < 3; ++component) {
            primitiveMin[component] =
                std::min(primitiveMin[component], vertex.position[component]);
            primitiveMax[component] =
                std::max(primitiveMax[component], vertex.position[component]);
        }
    }
    for (std::size_t component = 0; component < 3; ++component) {
        draw.center[component] =
            (primitiveMin[component] + primitiveMax[component]) * 0.5F;
    }
    asset.vertices.insert(asset.vertices.end(), vertices.begin(), vertices.end());
    asset.indices.insert(asset.indices.end(), indices.begin(), indices.end());
    asset.primitives.push_back(draw);
}

void visitNode(
    const tinygltf::Model& model,
    const int nodeIndex,
    const Matrix4& parentTransform,
    const std::uint32_t fallbackMaterial,
    LoadedAsset& asset) {
    const auto& node = model.nodes.at(static_cast<std::size_t>(nodeIndex));
    const Matrix4 worldTransform = multiply(parentTransform, nodeTransform(node));
    if (node.mesh >= 0) {
        const bool skinned = node.skin >= 0;
        if (skinned && node.skin != 0) {
            throw std::runtime_error(
                "The current renderer supports only glTF skin index 0");
        }
        const auto& mesh = model.meshes.at(static_cast<std::size_t>(node.mesh));
        for (const auto& primitive : mesh.primitives) {
            appendPrimitive(
                model,
                primitive,
                worldTransform,
                skinned,
                fallbackMaterial,
                asset);
        }
    }
    for (const int child : node.children) {
        visitNode(model, child, worldTransform, fallbackMaterial, asset);
    }
}

}  // namespace

LoadedAsset loadGltfAsset(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warning;
    std::string error;
    const std::filesystem::path filePath(path);
    const bool loaded = filePath.extension() == ".glb"
        ? loader.LoadBinaryFromFile(&model, &error, &warning, path)
        : loader.LoadASCIIFromFile(&model, &error, &warning, path);
    if (!warning.empty()) {
        std::cerr << "[glTF warning] " << warning << '\n';
    }
    if (!loaded) {
        throw std::runtime_error("Unable to load glTF asset: " + error);
    }

    LoadedAsset asset;
    asset.materials.reserve(model.materials.size() + 1);
    for (const auto& material : model.materials) {
        asset.materials.push_back(loadMaterial(model, &material));
    }
    const std::uint32_t fallbackMaterial =
        static_cast<std::uint32_t>(asset.materials.size());
    asset.materials.push_back(loadMaterial(model, nullptr));
    const std::vector<Matrix4> nodeWorldTransforms =
        calculateNodeWorldTransforms(model);
    loadJointMatrices(model, nodeWorldTransforms, asset);

    if (!model.scenes.empty()) {
        const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        const auto& scene = model.scenes.at(static_cast<std::size_t>(sceneIndex));
        for (const int node : scene.nodes) {
            visitNode(model, node, identityMatrix(), fallbackMaterial, asset);
        }
    } else {
        for (std::size_t index = 0; index < model.nodes.size(); ++index) {
            if (model.nodes[index].mesh >= 0) {
                visitNode(
                    model,
                    static_cast<int>(index),
                    identityMatrix(),
                    fallbackMaterial,
                    asset);
            }
        }
    }

    if (asset.primitives.empty()) {
        throw std::runtime_error("glTF scene contains no triangle mesh primitive");
    }
    asset.boundsMin.fill(std::numeric_limits<float>::max());
    asset.boundsMax.fill(std::numeric_limits<float>::lowest());
    for (const auto& vertex : asset.vertices) {
        for (std::size_t component = 0; component < 3; ++component) {
            asset.boundsMin[component] =
                std::min(asset.boundsMin[component], vertex.position[component]);
            asset.boundsMax[component] =
                std::max(asset.boundsMax[component], vertex.position[component]);
        }
    }
    return asset;
}
