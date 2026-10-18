#include "GltfLoader.hpp"
#include "assets/image/ImageCodec.hpp"
#include "BrowRegions.hpp"
#include "scene/TransformParity.hpp"
#include "scene/TransformEditing.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "assets/GltfAccessorValidation.hpp"

namespace {

using Matrix4 = std::array<double, 16>;

std::array<float, 4> readMaterialVectorExtra(
    const tinygltf::Value& extras,
    const std::string& key,
    const std::array<float, 4>& fallback);

AssetMaterialClass materialClassFromName(const std::string& value) {
    std::string normalized = value;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    if (normalized == "skin") return AssetMaterialClass::Skin;
    if (normalized == "face") return AssetMaterialClass::Face;
    if (normalized == "hair") return AssetMaterialClass::Hair;
    if (normalized == "fabric") return AssetMaterialClass::Fabric;
    if (normalized == "metal") return AssetMaterialClass::Metal;
    if (normalized == "eye") return AssetMaterialClass::Eye;
    if (normalized == "overlay") return AssetMaterialClass::Overlay;
    if (normalized == "emissive") return AssetMaterialClass::Emissive;
    if (normalized == "showcase") return AssetMaterialClass::Showcase;
    return AssetMaterialClass::Generic;
}

std::uint32_t defaultMaterialFeatures(const AssetMaterialClass value) {
    switch (value) {
        case AssetMaterialClass::Hair:
            return MaterialFeatureStylizedShadow
                | MaterialFeatureHairAnisotropy;
        case AssetMaterialClass::Face:
            return MaterialFeatureStylizedShadow
                | MaterialFeatureFaceSdfEligible;
        case AssetMaterialClass::Overlay:
            return MaterialFeatureOverlay;
        case AssetMaterialClass::Emissive:
            return MaterialFeatureEmissiveMask;
        case AssetMaterialClass::Showcase:
            return 0;
        case AssetMaterialClass::Generic:
            return MaterialFeatureNeutralFallback;
        default:
            return MaterialFeatureStylizedShadow;
    }
}

bool isBrowMaterialName(const std::string& name) {
    std::string normalized = name;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return normalized.find("brow") != std::string::npos;
}

std::array<float, 4> defaultStyleParameters(
    const AssetMaterialClass value) {
    switch (value) {
        case AssetMaterialClass::Skin: return {0.90F, 0.80F, 0.35F, 0.35F};
        case AssetMaterialClass::Face: return {0.85F, 0.75F, 0.15F, 0.25F};
        case AssetMaterialClass::Hair: return {1.00F, 1.00F, 0.40F, 0.65F};
        case AssetMaterialClass::Fabric: return {1.00F, 1.00F, 0.60F, 0.50F};
        case AssetMaterialClass::Metal: return {0.85F, 0.75F, 1.35F, 0.75F};
        case AssetMaterialClass::Eye: return {0.65F, 0.50F, 0.65F, 0.25F};
        case AssetMaterialClass::Overlay: return {0.00F, 0.00F, 0.00F, 0.00F};
        case AssetMaterialClass::Emissive: return {0.50F, 0.00F, 0.00F, 0.00F};
        default: return {1.00F, 1.00F, 1.00F, 1.00F};
    }
}

std::array<float, 4> defaultFeatureParameters(
    const AssetMaterialClass value) {
    switch (value) {
        case AssetMaterialClass::Skin: return {0.75F, 0.00F, 1.00F, 0.00F};
        case AssetMaterialClass::Face: return {0.65F, 0.00F, 1.00F, 1.00F};
        case AssetMaterialClass::Hair: return {0.85F, 1.00F, 1.00F, 0.00F};
        case AssetMaterialClass::Eye: return {0.50F, 0.00F, 1.00F, 0.00F};
        case AssetMaterialClass::Overlay: return {0.00F, 0.00F, 0.00F, 0.00F};
        case AssetMaterialClass::Emissive: return {0.50F, 0.00F, 1.50F, 0.00F};
        default: return {1.00F, 0.00F, 1.00F, 0.00F};
    }
}

AssetMaterialClass inferLegacyMaterialClass(const std::string& name) {
    std::string normalized = name;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    if (normalized.find("hairshadow") != std::string::npos
        || normalized.find("eyeshadow") != std::string::npos
        || normalized.find("brow") != std::string::npos) {
        return AssetMaterialClass::Overlay;
    }
    if (normalized.find("face") != std::string::npos) {
        return AssetMaterialClass::Face;
    }
    if (normalized.find("iris") != std::string::npos
        || normalized.find("eye") != std::string::npos) {
        return AssetMaterialClass::Eye;
    }
    if (normalized.find("hair") != std::string::npos) {
        return AssetMaterialClass::Hair;
    }
    if (normalized.find("body") != std::string::npos
        || normalized.find("skin") != std::string::npos) {
        return AssetMaterialClass::Skin;
    }
    if (normalized.find("cloth") != std::string::npos
        || normalized.find("fabric") != std::string::npos) {
        return AssetMaterialClass::Fabric;
    }
    if (normalized.find("metal") != std::string::npos) {
        return AssetMaterialClass::Metal;
    }
    return AssetMaterialClass::Generic;
}

std::uint32_t materialFeatureFromName(const std::string& value) {
    if (value == "stylized-shadow") return MaterialFeatureStylizedShadow;
    if (value == "hair-anisotropy") return MaterialFeatureHairAnisotropy;
    if (value == "face-sdf-eligible") return MaterialFeatureFaceSdfEligible;
    if (value == "emissive-mask") return MaterialFeatureEmissiveMask;
    if (value == "overlay") return MaterialFeatureOverlay;
    if (value == "neutral-fallback") return MaterialFeatureNeutralFallback;
    if (value == "brow-overlay") return MaterialFeatureBrowOverlay;
    if (value == "surface-ao") return MaterialFeatureSurfaceAo;
    if (value == "scene-tint") return MaterialFeatureSceneTint;
    if (value == "hair-ramp") return MaterialFeatureHairRamp;
    if (value == "scene-refraction") return MaterialFeatureSceneRefraction;
    return 0;
}

void loadMaterialProfile(
    const tinygltf::Material& material,
    AssetMaterial& result) {
    result.name = material.name.empty() ? "UnnamedMaterial" : material.name;
    if(material.extras.IsObject()&&material.extras.Has("azureSlotId")){
        const auto& identity=material.extras.Get("azureSlotId");
        if(!identity.IsString()||identity.Get<std::string>().empty())throw std::invalid_argument("azureSlotId requires a nonempty string");
        result.slotId=identity.Get<std::string>();
    }
    result.materialClass = inferLegacyMaterialClass(result.name);
    result.materialFeatures = defaultMaterialFeatures(result.materialClass);
    if (isBrowMaterialName(result.name)) {
        result.materialFeatures |= MaterialFeatureBrowOverlay;
        // Brow-only card expansion in metres and Face-D reconstruction radius.
        result.styleParameters = {0.0012F, 0.0F, 0.0F, 0.0F};
        // Unreal centimetres converted to glTF metres, then opacity,
        // fade distance and base power.
        result.featureParameters = {0.02F, 0.95F, 0.02F, 1.0F};
    }
    if (!material.extras.IsObject()
        || !material.extras.Has("azureRenderMaterial")) {
        return;
    }
    const tinygltf::Value& profile =
        material.extras.Get("azureRenderMaterial");
    if (!profile.IsObject()) {
        throw std::runtime_error(
            "azureRenderMaterial must be an object for " + result.name);
    }
    if (!profile.Has("schemaVersion")
        || !profile.Get("schemaVersion").IsNumber()
        || static_cast<int>(
            profile.Get("schemaVersion").GetNumberAsDouble()) < 1 || static_cast<int>(profile.Get("schemaVersion").GetNumberAsDouble()) > 2) {
        throw std::runtime_error(
            "Unsupported azureRenderMaterial schemaVersion for "
            + result.name);
    }
    if (!profile.Has("class") || !profile.Get("class").IsString()) {
        throw std::runtime_error(
            "azureRenderMaterial.class is required for " + result.name);
    }
    result.materialProfileVersion = static_cast<std::uint32_t>(profile.Get("schemaVersion").GetNumberAsDouble());
    result.materialProfileExplicit = true;
    const std::string className = profile.Get("class").Get<std::string>();
    result.materialClass = materialClassFromName(className);
    std::string normalizedClassName = className;
    std::transform(
        normalizedClassName.begin(),
        normalizedClassName.end(),
        normalizedClassName.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    if (result.materialClass == AssetMaterialClass::Generic
        && normalizedClassName != "generic") {
        throw std::runtime_error(
            "Unknown azureRenderMaterial class '" + className
            + "' for " + result.name);
    }
    result.materialFeatures = defaultMaterialFeatures(result.materialClass);
    result.styleParameters = defaultStyleParameters(result.materialClass);
    result.featureParameters = defaultFeatureParameters(result.materialClass);
    result.styleParameters = readMaterialVectorExtra(
        profile, "styleParameters", result.styleParameters);
    result.featureParameters = readMaterialVectorExtra(
        profile, "featureParameters", result.featureParameters);
    if (profile.Has("features")) {
        const tinygltf::Value& features = profile.Get("features");
        if (!features.IsArray()) {
            throw std::runtime_error(
                "azureRenderMaterial.features must be an array for "
                + result.name);
        }
        result.materialFeatures = 0;
        for (std::size_t index = 0; index < features.ArrayLen(); ++index) {
            const tinygltf::Value& feature = features.Get(index);
            if (!feature.IsString()) {
                throw std::runtime_error(
                    "azureRenderMaterial feature must be a string for "
                    + result.name);
            }
            const std::string featureName = feature.Get<std::string>();
            const std::uint32_t flag = materialFeatureFromName(featureName);
            if (flag == 0) {
                throw std::runtime_error(
                    "Unknown azureRenderMaterial feature '" + featureName
                    + "' for " + result.name);
            }
            result.materialFeatures |= flag;
        }
    }
    if ((result.materialFeatures & MaterialFeatureBrowOverlay) && result.materialProfileVersion == 1) {
        result.featureParameters[0] = std::min(result.featureParameters[0], 0.02F);
        result.styleParameters[0] = std::min(result.styleParameters[0], 0.0012F);
    }
}

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

void transformVertex(AssetVertex& vertex, const Matrix4& transform, std::array<float,2>* directionLengths=nullptr) {
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
    if(directionLengths)*directionLengths={static_cast<float>(length),static_cast<float>(tangentLength)};
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
        || tinygltf::GetNumComponentsInType(static_cast<std::uint32_t>(accessor.type)) != static_cast<int>(componentCount)
        || accessor.count != vertices.size()) {
        throw std::runtime_error(std::string("Unsupported glTF attribute layout: ") + name);
    }

    const unsigned char* source = accessorData(model, accessor);
    const std::size_t stride = accessorStride(model, accessor);
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        std::array<float, 4> values{};
        std::memcpy(values.data(), source + index * stride, componentCount * sizeof(float));
        for(std::size_t c=0;c<componentCount;++c)if(!std::isfinite(values[c]))throw std::invalid_argument("Vertex attribute must be finite");
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

void copyMorphTargets(const tinygltf::Model& model,const tinygltf::Primitive& primitive,
    std::vector<AssetVertex>& vertices,std::uint32_t& morphCount) {
    morphCount=static_cast<std::uint32_t>(primitive.targets.size());
    // The first two position streams retain the legacy vertex layout.
    for(std::size_t t=0;t<std::min<std::size_t>(2,primitive.targets.size());++t){
        const auto found=primitive.targets[t].find("POSITION");if(found==primitive.targets[t].end())continue;
        const auto& a=model.accessors.at(found->second);
        if(a.type!=TINYGLTF_TYPE_VEC3||a.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT||a.count!=vertices.size())
            throw std::invalid_argument("Invalid morph POSITION accessor");
        for(std::size_t v=0;v<vertices.size();++v)
            std::memcpy((t?vertices[v].morph1:vertices[v].morph0).data(),accessorData(model,a)+v*accessorStride(model,a),12);
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

struct AuthoredImageData{std::vector<azurerender::ImageMipLevel> levels;std::string compression;};
using AuthoredImageMips=std::map<int,AuthoredImageData>;
std::vector<std::uint8_t> decodeImageRgba(
    const tinygltf::Model& model,
    const int imageIndex,
    const std::array<double, 4>& factor,
    std::uint32_t& width,
    std::uint32_t& height,bool encodedColor=true,const AuthoredImageMips* authored=nullptr,unsigned* levelCount=nullptr,bool normalUsage=false) {
    const auto& image = model.images.at(static_cast<std::size_t>(imageIndex));
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
        throw std::runtime_error("glTF material image did not decode");
    }
    width = static_cast<std::uint32_t>(image.width);
    height = static_cast<std::uint32_t>(image.height);
    azurerender::DecodedImage typed;typed.width=width;typed.height=height;typed.sourceChannels=4;
    const auto count=static_cast<std::size_t>(width)*height*4;
    if(image.component!=4)throw std::runtime_error("Material decoder requires RGBA image data");
    if(image.bits==8)typed.rgba8=image.image;
    else if(image.bits==16) {
        if(image.image.size()!=count*2)throw std::runtime_error("Invalid sixteen-bit image storage");
        typed.precision=azurerender::ImagePrecision::UInt16;typed.rgba16.resize(count);std::memcpy(typed.rgba16.data(),image.image.data(),count*2);
    } else if(image.bits==32) {
        if(image.image.size()!=count*4)throw std::runtime_error("Invalid float image storage");
        typed.precision=azurerender::ImagePrecision::Float32;typed.colorSpace=azurerender::ImageColorSpace::Linear;
        typed.rgba32.resize(count);std::memcpy(typed.rgba32.data(),image.image.data(),count*4);
    } else throw std::runtime_error("Unsupported image precision");
    const auto converted=azurerender::convertImageToRgba8(typed,encodedColor);
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * height * 4);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height;
         ++pixel) {
        const std::size_t source = pixel * static_cast<std::size_t>(image.component);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::uint8_t sourceValue = channel < static_cast<std::size_t>(image.component)
                ? converted.pixels[source + channel]
                : (channel == 3 ? 255 : converted.pixels[source]);
            pixels[pixel * 4 + channel] = static_cast<std::uint8_t>(
                static_cast<double>(sourceValue) * std::clamp(factor[channel], 0.0, 1.0));
        }
    }
    if(authored){const auto found=authored->find(imageIndex);if(found!=authored->end()){
        if(levelCount)*levelCount=static_cast<unsigned>(found->second.levels.size());
        for(std::size_t level=1;level<found->second.levels.size();++level){const auto& samples=found->second.levels[level].rgba8;for(std::size_t i=0;i<samples.size();++i)pixels.push_back(static_cast<std::uint8_t>(samples[i]*std::clamp(factor[i%4],0.,1.)));}
        if(normalUsage&&found->second.compression=="bc5")azurerender::reconstructBc5NormalPixels(pixels);
    }}
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

int loadSceneRefraction(const tinygltf::Material& material,
    const tinygltf::Model& model, AssetMaterial& result) {
    if(!material.extras.IsObject() || !material.extras.Has("azureRenderMaterial"))return -1;
    const auto& profile=material.extras.Get("azureRenderMaterial");
    const bool enabled=(result.materialFeatures&MaterialFeatureSceneRefraction)!=0;
    if(!enabled && !profile.Has("sceneRefraction"))return -1;
    if(!enabled || !profile.Has("sceneRefraction") || result.materialProfileVersion!=2
        || result.materialClass!=AssetMaterialClass::Generic || result.alphaMode!=AssetAlphaMode::Blend
        || (result.materialFeatures&~(MaterialFeatureSceneRefraction|MaterialFeatureSurfaceAo|MaterialFeatureEmissiveMask)))
        throw std::invalid_argument("sceneRefraction requires generic schema 2, transparent queue and compatible features");
    const auto& glass=profile.Get("sceneRefraction");
    if(!glass.IsObject())throw std::invalid_argument("sceneRefraction must be an object");
    const auto scalar=[&](const char* name,float fallback,float maximum) {
        if(!glass.Has(name))return fallback;
        const auto& v=glass.Get(name);
        if(!v.IsNumber())throw std::invalid_argument("Invalid sceneRefraction numeric field");
        const double number=v.GetNumberAsDouble();
        if(!std::isfinite(number)||number<0||number>maximum)throw std::invalid_argument("sceneRefraction field outside range");
        return static_cast<float>(number);
    };
    const auto vector=[&](const char* name,std::array<float,4> fallback) {
        if(!glass.Has(name))return fallback;
        const auto& v=glass.Get(name);
        if(!v.IsArray()||v.ArrayLen()!=4)throw std::invalid_argument("sceneRefraction vector requires four numbers");
        for(unsigned i=0;i<4;++i) {
            const auto& c=v.Get(static_cast<int>(i));
            if(!c.IsNumber()||!std::isfinite(c.GetNumberAsDouble())||std::abs(c.GetNumberAsDouble())>65536)
                throw std::invalid_argument("Invalid sceneRefraction vector component");
            fallback[i]=static_cast<float>(c.GetNumberAsDouble());
        }
        return fallback;
    };
    // Material constants form a per-feature union. Public authoring uses the
    // named object; glass reserves these slots instead of character styling.
    result.styleParameters=vector("uvTransform",{1,1,0,0});
    result.matcapColor=vector("tint",{1,1,1,1});
    for(float c:result.matcapColor)if(c<0)throw std::invalid_argument("Refraction tint must be nonnegative");
    result.hairParameters={scalar("contribution",.8F,1),scalar("brightness",1,64),scalar("distortionStrength",0,1),0};
    result.featureParameters={0,0,0,0};
    if(!glass.Has("texture"))throw std::invalid_argument("sceneRefraction requires a distortion texture");
    const auto& texture=glass.Get("texture");
    const double index=texture.IsNumber()?texture.GetNumberAsDouble():-1;
    if(!std::isfinite(index)||index<0||std::floor(index)!=index||index>=model.textures.size())
        throw std::invalid_argument("Invalid sceneRefraction texture index");
    const int image=model.textures[static_cast<std::size_t>(index)].source;
    if(image<0||static_cast<std::size_t>(image)>=model.images.size())
        throw std::invalid_argument("sceneRefraction distortion image is missing");
    return image;
}

AssetMaterial loadMaterial(
    const tinygltf::Model& model,
    const tinygltf::Material* material,const AuthoredImageMips* authored=nullptr) {
    AssetMaterial result;
    int baseColorImage = -1;
    int normalImage = -1;
    int metallicRoughnessImage = -1;
    int specularEmissiveImage = -1;
    int styleMaskImage = -1;
    int matcapImage = -1;
    int hairDataImage = -1;
    int faceSdfImage = -1;
    std::array<double, 4> factor{1.0, 1.0, 1.0, 1.0};
    if (material != nullptr) {
        loadMaterialProfile(*material, result);
        const auto scalar=[&](const char* extension,const char* field,float fallback){
            const auto it=material->extensions.find(extension);if(it==material->extensions.end()||!it->second.Has(field))return fallback;
            const auto& v=it->second.Get(field);if(!v.IsNumber()||!std::isfinite(v.GetNumberAsDouble()))throw std::runtime_error("Invalid physical material field");return static_cast<float>(v.GetNumberAsDouble());};
        result.transmission=scalar("KHR_materials_transmission","transmissionFactor",0);
        result.ior=scalar("KHR_materials_ior","ior",1.5F);result.thickness=scalar("KHR_materials_volume","thicknessFactor",0);
        result.attenuationDistance=scalar("KHR_materials_volume","attenuationDistance",0);
        if(result.transmission<0||result.transmission>1||result.ior<1||result.ior>3||result.thickness<0||result.attenuationDistance<0)throw std::runtime_error("Physical material parameter outside supported range");
        const auto volume=material->extensions.find("KHR_materials_volume");
        if(volume!=material->extensions.end()&&volume->second.Has("attenuationColor")){
            const auto& color=volume->second.Get("attenuationColor");if(!color.IsArray()||color.ArrayLen()!=3)throw std::runtime_error("Invalid attenuation color");
            for(unsigned channel=0;channel<3;++channel){if(!color.Get(static_cast<int>(channel)).IsNumber())throw std::runtime_error("Invalid attenuation channel");result.attenuationColor[channel]=static_cast<float>(color.Get(static_cast<int>(channel)).GetNumberAsDouble());if(result.attenuationColor[channel]<0||result.attenuationColor[channel]>1)throw std::runtime_error("Attenuation color outside unit range");}
        }
        const auto transmission=material->extensions.find("KHR_materials_transmission");
        if(transmission!=material->extensions.end()&&transmission->second.Has("transmissionTexture")){
            const auto& info=transmission->second.Get("transmissionTexture");const int texture=info.Get("index").GetNumberAsInt();
            if(info.Has("texCoord")&&info.Get("texCoord").GetNumberAsInt()!=0)throw std::runtime_error("Transmission requires texture coordinates zero");
            result.transmissionPixels=decodeImageRgba(model,model.textures.at(static_cast<std::size_t>(texture)).source,{1,1,1,1},result.transmissionWidth,result.transmissionHeight,false,authored,nullptr);
        }
        for(const auto& extension:material->extensions)if(extension.first!="KHR_materials_transmission"&&extension.first!="KHR_materials_volume"&&extension.first!="KHR_materials_ior"&&extension.first!="KHR_materials_emissive_strength")
            azurerender::RuntimeDiagnostics::instance().warning("asset","Unsupported material extension: "+extension.first);

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
            if (result.emissiveStrength > 0.0F) {
                result.materialFeatures |= MaterialFeatureEmissiveMask;
            }
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
        if(result.materialClass==AssetMaterialClass::Hair && result.materialProfileVersion<2)
            result.hairParameters[3]=.1F;
        if(material->extras.IsObject() && material->extras.Has("azureRenderMaterial")){
            const auto& p=material->extras.Get("azureRenderMaterial");
            if(p.Has("hair")){
                const auto& h=p.Get("hair");
                if(!h.IsObject() || !h.Has("power") || !h.Has("strength") || !h.Has("rampRow") || !h.Has("strandShift"))
                    throw std::runtime_error("Incomplete hair profile: "+result.name);
                const char* fields[]={"power","strength","rampRow","strandShift"};
                for(unsigned i=0;i<4;++i){if(!h.Get(fields[i]).IsNumber())throw std::runtime_error("Hair profile requires numeric fields");result.hairParameters[i]=static_cast<float>(h.Get(fields[i]).GetNumberAsDouble());}
                auto& v=result.hairParameters;
                if(!std::isfinite(v[0]) || v[0]<1 || v[0]>2048 || !std::isfinite(v[1]) || v[1]<0 || v[1]>4 || !std::isfinite(v[2]) || v[2]<0 || v[2]>9 || std::floor(v[2])!=v[2] || !std::isfinite(v[3]) || std::abs(v[3])>1)
                    throw std::runtime_error("Hair profile is outside supported ranges");
                if(h.Has("cameraOffset")){
                    const auto& offset=h.Get("cameraOffset");
                    if(!offset.IsArray() || offset.ArrayLen()!=3)throw std::runtime_error("Hair cameraOffset requires three components");
                    for(unsigned i=0;i<3;++i){if(!offset.Get(i).IsNumber())throw std::runtime_error("Hair cameraOffset must be numeric");result.hairViewParameters[i]=static_cast<float>(offset.Get(i).GetNumberAsDouble());}
                }
                if(h.Has("upperLimit")){if(!h.Get("upperLimit").IsNumber())throw std::runtime_error("Hair upperLimit must be numeric");result.hairViewParameters[3]=static_cast<float>(h.Get("upperLimit").GetNumberAsDouble());}
                for(float value:result.hairViewParameters)if(!std::isfinite(value)||std::abs(value)>16)throw std::runtime_error("Hair view profile is outside supported ranges");
                if(result.hairViewParameters[3]<=0)throw std::runtime_error("Hair highlight limit must be positive");
            }
        }
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
        if (material->extras.IsObject()
            && material->extras.Has("azureRenderMaterial")) {
            const tinygltf::Value& profile =
                material->extras.Get("azureRenderMaterial");
            if (profile.IsObject() && profile.Has("hair") && profile.Get("hair").Has("rampTexture")) {
                const auto& ramp=profile.Get("hair").Get("rampTexture");
                const double index=ramp.IsNumber()?ramp.GetNumberAsDouble():-1;
                if (!std::isfinite(index) || std::floor(index)!=index || index<0 || index>=model.textures.size()
                    || result.materialClass!=AssetMaterialClass::Hair || !(result.materialFeatures&MaterialFeatureHairRamp)) {
                    throw std::runtime_error("Invalid hair.rampTexture for "+result.name);
                }
                // Hair ramp and matcap are exclusive material-class uses of this slot.
                matcapImage=model.textures[static_cast<std::size_t>(index)].source;
            }
            if (profile.IsObject() && profile.Has("faceSdf")) {
                const tinygltf::Value& faceSdf = profile.Get("faceSdf");
                if (!faceSdf.IsObject()) {
                    throw std::runtime_error(
                        "azureRenderMaterial.faceSdf must be an object for "
                        + result.name);
                }
                if ((result.materialFeatures
                     & MaterialFeatureFaceSdfEligible) == 0) {
                    throw std::runtime_error(
                        "faceSdf requires the face-sdf-eligible feature for "
                        + result.name);
                }
                const auto requireNumber = [&](const char* key) -> int {
                    if (!faceSdf.Has(key) || !faceSdf.Get(key).IsNumber()) {
                        throw std::runtime_error(
                            std::string("faceSdf.") + key
                            + " must be a number for " + result.name);
                    }
                    const double value =
                        faceSdf.Get(key).GetNumberAsDouble();
                    if (!std::isfinite(value) || std::floor(value) != value
                        || value < static_cast<double>(
                            std::numeric_limits<int>::min())
                        || value > static_cast<double>(
                            std::numeric_limits<int>::max())) {
                        throw std::runtime_error(
                            std::string("faceSdf.") + key
                            + " must be an integer for " + result.name);
                    }
                    return static_cast<int>(value);
                };
                const auto requireString = [&](const char* key) -> std::string {
                    if (!faceSdf.Has(key) || !faceSdf.Get(key).IsString()) {
                        throw std::runtime_error(
                            std::string("faceSdf.") + key
                            + " must be a string for " + result.name);
                    }
                    return faceSdf.Get(key).Get<std::string>();
                };
                if (requireNumber("schemaVersion")
                    != static_cast<int>(AssetFaceSdfProfile::kSchemaVersion)) {
                    throw std::runtime_error(
                        "Unsupported faceSdf schemaVersion for "
                        + result.name);
                }
                const int textureIndex = requireNumber("texture");
                if (textureIndex < 0
                    || static_cast<std::size_t>(textureIndex)
                        >= model.textures.size()) {
                    throw std::runtime_error(
                        "faceSdf.texture is out of range for " + result.name);
                }
                faceSdfImage = model.textures[
                    static_cast<std::size_t>(textureIndex)].source;
                if (faceSdfImage < 0
                    || static_cast<std::size_t>(faceSdfImage)
                        >= model.images.size()) {
                    throw std::runtime_error(
                        "faceSdf.texture has no valid image for "
                        + result.name);
                }
                if (requireNumber("texCoord") != 0) {
                    throw std::runtime_error(
                        "Face SDF v1 supports only TEXCOORD_0 for "
                        + result.name);
                }
                const std::string channel = requireString("channel");
                const auto parseChannel = [&](const std::string& value,
                                              const char* key) {
                    if (value == "r") return AssetFaceSdfChannel::Red;
                    if (value == "g") return AssetFaceSdfChannel::Green;
                    if (value == "b") return AssetFaceSdfChannel::Blue;
                    if (value == "a") return AssetFaceSdfChannel::Alpha;
                    throw std::runtime_error(
                        std::string("Unknown faceSdf.") + key
                        + " for " + result.name);
                };
                result.faceSdf.channel = parseChannel(channel, "channel");
                result.faceSdf.maskChannel = parseChannel(
                    requireString("maskChannel"),
                    "maskChannel");
                if (!faceSdf.Has("shadowOnLowValues")
                    || !faceSdf.Get("shadowOnLowValues").IsBool()) {
                    throw std::runtime_error(
                        "faceSdf.shadowOnLowValues must be boolean for "
                        + result.name);
                }
                result.faceSdf.shadowOnLowValues =
                    faceSdf.Get("shadowOnLowValues").Get<bool>();
                const std::string horizontalAxis =
                    requireString("horizontalAxis");
                if (horizontalAxis == "left-to-right") {
                    result.faceSdf.mirrorHorizontal = false;
                } else if (horizontalAxis == "right-to-left") {
                    result.faceSdf.mirrorHorizontal = true;
                } else {
                    throw std::runtime_error(
                        "Unknown faceSdf.horizontalAxis for " + result.name);
                }
                result.faceSdf.headNodeName = requireString("headNode");
                std::optional<std::uint32_t> headNode;
                for (std::size_t nodeIndex = 0;
                     nodeIndex < model.nodes.size();
                     ++nodeIndex) {
                    if (model.nodes[nodeIndex].name
                        != result.faceSdf.headNodeName) {
                        continue;
                    }
                    if (headNode.has_value()) {
                        throw std::runtime_error(
                            "faceSdf.headNode is not unique for "
                            + result.name);
                    }
                    headNode = static_cast<std::uint32_t>(nodeIndex);
                }
                if (!headNode.has_value()) {
                    throw std::runtime_error(
                        "faceSdf.headNode does not resolve for "
                        + result.name);
                }
                result.faceSdf.headNode = *headNode;
                result.faceSdf.present = true;
            }
        }
        result.alphaCutoff = static_cast<float>(material->alphaCutoff);
        result.doubleSided = material->doubleSided;
        if (material->alphaMode == "MASK") {
            result.alphaMode = AssetAlphaMode::Mask;
        } else if (material->alphaMode == "BLEND") {
            result.alphaMode = AssetAlphaMode::Blend;
        }
        const int distortionImage=loadSceneRefraction(*material,model,result);
        if(distortionImage>=0)styleMaskImage=distortionImage;
    }

    if (baseColorImage >= 0) {
        result.baseColorPixels = decodeImageRgba(
            model,
            baseColorImage,
            {1.0, 1.0, 1.0, 1.0},
            result.baseColorWidth,
            result.baseColorHeight,true,authored,&result.authoredMipLevels[0]);
    } else {
        result.baseColorWidth = 2;
        result.baseColorHeight = 2;
        result.baseColorPixels.resize(16);
        for (std::size_t pixel = 0; pixel < 4; ++pixel) {
            for (std::size_t channel = 0; channel < 4; ++channel) {
                result.baseColorPixels[pixel * 4 + channel] =
                    static_cast<std::uint8_t>(
                        (channel == 3 ? std::clamp(factor[channel], 0.0, 1.0) : (factor[channel] <= 0.0031308 ? 12.92 * factor[channel] : 1.055 * std::pow(factor[channel], 1.0 / 2.4) - 0.055)) * 255.0);
            }
        }
    }

    if (baseColorImage >= 0) {
        for (std::size_t i = 0; i < result.baseColorPixels.size(); ++i) {
            if (factor[i % 4] == 1.0) continue;
            const double value = result.baseColorPixels[i] / 255.0;
            const double linear = i % 4 == 3 ? value : (value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4));
            const double scaled = linear * std::clamp(factor[i % 4], 0.0, 1.0);
            const double encoded = i % 4 == 3 ? scaled : (scaled <= .0031308 ? scaled * 12.92 : 1.055 * std::pow(scaled, 1.0 / 2.4) - .055);
            result.baseColorPixels[i] = static_cast<std::uint8_t>(std::round(std::clamp(encoded, 0.0, 1.0) * 255.0));
        }
    }
    if (normalImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.normalPixels = decodeImageRgba(
            model,
            normalImage,
            kNoFactor,
            result.normalWidth,
            result.normalHeight,false,authored,&result.authoredMipLevels[1],true);
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
            result.metallicRoughnessHeight,false,authored,&result.authoredMipLevels[2]);
    } else {
        result.metallicRoughnessWidth = 2;
        result.metallicRoughnessHeight = 2;
        result.metallicRoughnessPixels = {
            255, 191, 0, 255, 255, 191, 0, 255,
            255, 191, 0, 255, 255, 191, 0, 255,
        };
    }
    const double roughnessFactor = material ? material->pbrMetallicRoughness.roughnessFactor : 1.0;
    const double metallicFactor = material ? material->pbrMetallicRoughness.metallicFactor : 1.0;
    for (std::size_t i = 0; i < result.metallicRoughnessPixels.size(); i += 4) {
        result.metallicRoughnessPixels[i+1] = static_cast<std::uint8_t>(std::round((metallicRoughnessImage >= 0 ? result.metallicRoughnessPixels[i+1] : 255) * std::clamp(roughnessFactor, 0.0, 1.0)));
        result.metallicRoughnessPixels[i+2] = static_cast<std::uint8_t>(std::round((metallicRoughnessImage >= 0 ? result.metallicRoughnessPixels[i+2] : 255) * std::clamp(metallicFactor, 0.0, 1.0)));
    }
    if (material && normalImage >= 0 && material->normalTexture.scale != 1.0) {
        for (std::size_t i=0;i<result.normalPixels.size();i+=4) {
            double x=(result.normalPixels[i]/255.0*2-1)*material->normalTexture.scale;
            double y=(result.normalPixels[i+1]/255.0*2-1)*material->normalTexture.scale;
            double z=result.normalPixels[i+2]/255.0*2-1;
            const double length=std::max(std::sqrt(x*x+y*y+z*z),1e-8);
            result.normalPixels[i]=static_cast<std::uint8_t>(std::round((x/length*.5+.5)*255));
            result.normalPixels[i+1]=static_cast<std::uint8_t>(std::round((y/length*.5+.5)*255));
            result.normalPixels[i+2]=static_cast<std::uint8_t>(std::round((z/length*.5+.5)*255));
        }
    }
    if (specularEmissiveImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.specularEmissivePixels = decodeImageRgba(
            model,
            specularEmissiveImage,
            kNoFactor,
            result.specularEmissiveWidth,
            result.specularEmissiveHeight,true,authored,&result.authoredMipLevels[3]);
    } else {
        result.specularEmissiveWidth = 2;
        result.specularEmissiveHeight = 2;
        result.specularEmissivePixels = {
            0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255,
        };
    }
    if(material&&result.emissiveStrength>0){
        for(std::size_t i=0;i<result.specularEmissivePixels.size();++i){const unsigned channel=static_cast<unsigned>(i%4);if(channel==3)continue;
            double value=specularEmissiveImage>=0?result.specularEmissivePixels[i]/255.:1.;value=value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);
            value*=material->emissiveFactor[channel]/result.emissiveStrength;value=value<=.0031308?value*12.92:1.055*std::pow(value,1/2.4)-.055;
            result.specularEmissivePixels[i]=static_cast<std::uint8_t>(std::round(std::clamp(value,0.,1.)*255));}
        const auto strength=material->extensions.find("KHR_materials_emissive_strength");if(strength!=material->extensions.end()&&strength->second.Has("emissiveStrength"))result.emissiveStrength*=static_cast<float>(strength->second.Get("emissiveStrength").GetNumberAsDouble());
    }
    if (styleMaskImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.styleMaskPixels = decodeImageRgba(
            model,
            styleMaskImage,
            kNoFactor,
            result.styleMaskWidth,
            result.styleMaskHeight,false,authored,&result.authoredMipLevels[4]);
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
            result.matcapHeight,true,authored,&result.authoredMipLevels[5]);
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
            result.hairDataHeight,false,authored,&result.authoredMipLevels[6]);
    } else {
        result.hairDataWidth = 2;
        result.hairDataHeight = 2;
        result.hairDataPixels = {
            128, 128, 128, 128, 128, 128, 128, 128,
            128, 128, 128, 128, 128, 128, 128, 128,
        };
    }
    if (faceSdfImage >= 0) {
        constexpr std::array<double, 4> kNoFactor{1.0, 1.0, 1.0, 1.0};
        result.faceSdf.pixels = decodeImageRgba(
            model,
            faceSdfImage,
            kNoFactor,
            result.faceSdf.width,
            result.faceSdf.height,false,authored,&result.authoredMipLevels[7]);
        const std::size_t distanceChannel =
            static_cast<std::size_t>(result.faceSdf.channel);
        const std::size_t maskChannel =
            static_cast<std::size_t>(result.faceSdf.maskChannel);
        for (std::size_t pixel = 0;
             pixel < result.faceSdf.pixels.size();
             pixel += 4) {
            std::uint8_t distance =
                result.faceSdf.pixels[pixel + distanceChannel];
            if (!result.faceSdf.shadowOnLowValues) {
                distance = static_cast<std::uint8_t>(255U - distance);
            }
            if (result.faceSdf.mirrorHorizontal) {
                distance = static_cast<std::uint8_t>(255U - distance);
            }
            const std::uint8_t mask =
                result.faceSdf.pixels[pixel + maskChannel];
            result.faceSdf.pixels[pixel] = distance;
            result.faceSdf.pixels[pixel + 1] = distance;
            result.faceSdf.pixels[pixel + 2] = distance;
            result.faceSdf.pixels[pixel + 3] = mask;
        }
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

void loadNodes(const tinygltf::Model& model, LoadedAsset& asset) {
    asset.nodes.resize(model.nodes.size());
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        const tinygltf::Node& source = model.nodes[index];
        AssetNode& destination = asset.nodes[index];
        destination.name = source.name;
        if (source.translation.size() == 3) {
            std::transform(
                source.translation.begin(),
                source.translation.end(),
                destination.translation.begin(),
                [](const double value) { return static_cast<float>(value); });
        }
        if (source.rotation.size() == 4) {
            std::transform(
                source.rotation.begin(),
                source.rotation.end(),
                destination.rotation.begin(),
                [](const double value) { return static_cast<float>(value); });
        }
        if (source.scale.size() == 3) {
            std::transform(
                source.scale.begin(),
                source.scale.end(),
                destination.scale.begin(),
                [](const double value) { return static_cast<float>(value); });
        }
        destination.usesMatrix = source.matrix.size() == 16;
        const Matrix4 local = nodeTransform(source);
        std::transform(
            local.begin(),
            local.end(),
            destination.localMatrix.begin(),
            [](const double value) { return static_cast<float>(value); });
        for (const int child : source.children) {
            asset.nodes.at(static_cast<std::size_t>(child)).parent =
                static_cast<std::int32_t>(index);
        }
    }
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
    asset.jointNodes.reserve(skin.joints.size());
    asset.inverseBindMatrices.reserve(skin.joints.size());
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
        std::array<float, 16> inverseBind{};
        std::transform(
            inverseBindMatrices[index].begin(),
            inverseBindMatrices[index].end(),
            inverseBind.begin(),
            [](const double value) { return static_cast<float>(value); });
        asset.jointNodes.push_back(
            static_cast<std::uint32_t>(jointNode));
        asset.inverseBindMatrices.push_back(inverseBind);
        asset.jointMatrices.push_back(gpuMatrix);
    }
    asset.hasSkin = true;
}

// Rigid mesh nodes share the deformation palette with real skin joints.
// Vertices retain their baked bind placement; pose * inverse(bind) supplies
// only the node's animated delta. Existing skin indices remain unchanged.
std::vector<int> loadRigidNodeJoints(const tinygltf::Model& model,
    const std::vector<Matrix4>& world, LoadedAsset& asset) {
    std::vector<int> mapping(model.nodes.size(),-1);
    if(model.animations.empty() && model.skins.empty())return mapping;
    for(std::size_t node=0;node<model.nodes.size();++node) {
        if(model.nodes[node].mesh<0 || model.nodes[node].skin>=0)continue;
        if(!asset.hasSkin)asset.jointMatrices.clear(); // Remove fallback identity.
        std::array<float,16> bind{};
        std::transform(world[node].begin(),world[node].end(),bind.begin(),
            [](double v){return static_cast<float>(v);});
        const auto inverse=azurerender::scene::inverseAffine(bind);
        mapping[node]=static_cast<int>(asset.jointMatrices.size());
        asset.jointNodes.push_back(static_cast<std::uint32_t>(node));
        asset.inverseBindMatrices.push_back(inverse);
        asset.jointMatrices.push_back({1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
        asset.hasSkin=true;
    }
    return mapping;
}

void loadAnimations(const tinygltf::Model& model,LoadedAsset& asset) {
    asset.defaultMorphWeights.resize(model.nodes.size());
    for(std::size_t i=0;i<model.nodes.size();++i){
        const auto& n=model.nodes[i];if(n.mesh<0)continue;const auto& m=model.meshes.at(n.mesh);
        const auto count=m.primitives.empty()?0:m.primitives.front().targets.size();
        if(count>kMorphTargetBudget)throw std::invalid_argument("glTF node "+std::to_string(i)+" has "+std::to_string(count)+" Morph targets; this renderer supports at most "+std::to_string(kMorphTargetBudget));
        for(const auto& primitive:m.primitives)if(primitive.targets.size()!=count)throw std::invalid_argument("Mesh primitive morph counts differ");
        const auto& source=n.weights.empty()?m.weights:n.weights;
        if(!source.empty()&&source.size()!=count)throw std::invalid_argument("Morph default weight count mismatch");
        auto& weights=asset.defaultMorphWeights[i];weights.assign(count,0);
        for(std::size_t t=0;t<source.size();++t){if(!std::isfinite(source[t]))throw std::invalid_argument("Morph default weight must be finite");weights[t]=static_cast<float>(source[t]);}
    }
    for(const auto& source:model.animations){
        AssetAnimation animation;animation.name=source.name;animation.startTime=std::numeric_limits<float>::max();
        for(const auto& c:source.channels){
            if(c.target_node<0||static_cast<std::size_t>(c.target_node)>=asset.nodes.size()||c.sampler<0)
                throw std::invalid_argument("Invalid animation channel target");
            AssetAnimationChannel channel;channel.nodeIndex=static_cast<std::uint32_t>(c.target_node);
            if(c.target_path=="translation")channel.path=AssetAnimationPath::Translation;
            else if(c.target_path=="rotation")channel.path=AssetAnimationPath::Rotation;
            else if(c.target_path=="scale")channel.path=AssetAnimationPath::Scale;
            else if(c.target_path=="weights")channel.path=AssetAnimationPath::Weights;
            else throw std::invalid_argument("Unsupported glTF animation target path");
            if(channel.path!=AssetAnimationPath::Weights&&asset.nodes[channel.nodeIndex].usesMatrix)throw std::invalid_argument("Animation cannot target a matrix-authored node");
            const auto& src=source.samplers.at(c.sampler);AssetAnimationSampler sampler;auto& curve=sampler.curve;
            curve.rotation=channel.path==AssetAnimationPath::Rotation;
            curve.width=channel.path==AssetAnimationPath::Weights?asset.defaultMorphWeights[channel.nodeIndex].size():curve.rotation?4:3;
            if(src.interpolation=="STEP"){curve.interpolation=CurveInterpolation::Step;sampler.interpolation=AssetAnimationInterpolation::Step;}
            else if(src.interpolation=="CUBICSPLINE"){curve.interpolation=CurveInterpolation::CubicSpline;sampler.interpolation=AssetAnimationInterpolation::CubicSpline;}
            else if(src.interpolation!="LINEAR"&&!src.interpolation.empty())throw std::invalid_argument("Unknown animation interpolation");
            const auto& input=model.accessors.at(src.input);const auto& output=model.accessors.at(src.output);
            const int expected=channel.path==AssetAnimationPath::Weights?TINYGLTF_TYPE_SCALAR:curve.rotation?TINYGLTF_TYPE_VEC4:TINYGLTF_TYPE_VEC3;
            if(input.type!=TINYGLTF_TYPE_SCALAR||input.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT||output.type!=expected||output.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT)
                throw std::invalid_argument("Animation accessor does not match target path");
            curve.times.resize(input.count);for(std::size_t i=0;i<input.count;++i)std::memcpy(&curve.times[i],accessorData(model,input)+i*accessorStride(model,input),4);
            const std::size_t components=channel.path==AssetAnimationPath::Weights?1:curve.width;
            curve.values.resize(output.count*components);for(std::size_t i=0;i<output.count;++i)std::memcpy(curve.values.data()+i*components,accessorData(model,output)+i*accessorStride(model,output),components*4);
            validateCurve(curve);sampler.inputTimes=curve.times;
            if(channel.path!=AssetAnimationPath::Weights){sampler.outputValues.resize(output.count);for(std::size_t i=0;i<output.count;++i)std::copy_n(curve.values.begin()+i*components,components,sampler.outputValues[i].begin());}
            animation.startTime=std::min(animation.startTime,curve.times.front());animation.endTime=std::max(animation.endTime,curve.times.back());
            channel.samplerIndex=static_cast<std::uint32_t>(animation.samplers.size());animation.samplers.push_back(std::move(sampler));animation.channels.push_back(channel);
        }
        if(animation.channels.empty())animation.startTime=animation.endTime=0;
        asset.animations.push_back(std::move(animation));
    }
}

void appendPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const Matrix4& transform,
    const bool skinned,
    const std::uint32_t fallbackMaterial,
    const std::uint32_t nodeIndex,
    const int rigidJoint,
    LoadedAsset& asset) {
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
        throw std::runtime_error("Only triangle-list glTF primitives are supported");
    }
    constexpr std::size_t kSupportedMorphTargetCount = kMorphTargetBudget;
    if (primitive.targets.size() > kSupportedMorphTargetCount) {
        throw std::runtime_error(
            "glTF primitive has "
            + std::to_string(primitive.targets.size())
            + " Morph targets; this renderer supports at most "
            + std::to_string(kSupportedMorphTargetCount));
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
    std::uint32_t morphCount = 0;
    copyMorphTargets(model, primitive, vertices, morphCount);
    if (morphCount > asset.morphTargetCount) {
        asset.morphTargetCount = morphCount;
    }
    std::vector<std::uint32_t> indices = readIndices(model, primitive, vertices.size());
    if (primitive.attributes.find("TANGENT") == primitive.attributes.end()) {
        generateTangents(vertices, indices);
    }
    std::vector<std::array<float,2>> directionLengths(vertices.size(),{1,1});
    if (!skinned) {
        for (std::size_t i=0;i<vertices.size();++i) {
            transformVertex(vertices[i], transform, &directionLengths[i]);
            if(rigidJoint>=0) {
                vertices[i].joints={static_cast<std::uint32_t>(rigidJoint),0,0,0};
                vertices[i].weights={1,0,0,0};
            }
        }
        if (azurerender::scene::mirroredTransform(transform)) {
            for (std::size_t i=0; i+2<indices.size(); i+=3)
                std::swap(indices[i+1],indices[i+2]);
        }
    } else {
        for (const AssetVertex& vertex : vertices) {
            for (const std::uint32_t joint : vertex.joints) {
                if (joint >= model.skins.at(0).joints.size()) {
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
    if(!primitive.targets.empty()) {
        MorphTargetRange range;range.node=nodeIndex;range.firstVertex=baseVertex;range.vertexCount=static_cast<std::uint32_t>(vertices.size());
        const auto& mesh=model.meshes.at(model.nodes.at(nodeIndex).mesh);
        for(std::size_t t=0;t<primitive.targets.size();++t){
            MorphTargetData target;target.name="target-"+std::to_string(t);
            if(mesh.extras.IsObject()&&mesh.extras.Has("targetNames")){
                const auto& names=mesh.extras.Get("targetNames");
                if(!names.IsArray()||names.ArrayLen()!=primitive.targets.size()||!names.Get(static_cast<int>(t)).IsString())throw std::invalid_argument("Invalid morph target names");
                target.name=names.Get(static_cast<int>(t)).Get<std::string>();
            }
            const char* semantics[]={"POSITION","NORMAL","TANGENT"};
            std::vector<std::array<float,3>>* outputs[]={&target.positions,&target.normals,&target.tangents};
            for(unsigned semantic=0;semantic<3;++semantic){
                auto& values=*outputs[semantic];values.resize(vertices.size());
                const auto found=primitive.targets[t].find(semantics[semantic]);if(found==primitive.targets[t].end())continue;
                const auto& a=model.accessors.at(found->second);
                if(a.type!=TINYGLTF_TYPE_VEC3||a.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT||a.count!=vertices.size())throw std::invalid_argument("Invalid morph target accessor");
                for(std::size_t v=0;v<values.size();++v){
                    std::memcpy(values[v].data(),accessorData(model,a)+v*accessorStride(model,a),12);
                    for(float value:values[v])if(!std::isfinite(value))throw std::invalid_argument("Morph delta must be finite");
                    if(!skinned){
                        const auto delta=values[v];
                        // Position and tangent deltas transform as vectors. Normal deltas
                        // use the same inverse transpose as the base normal.
                        if(semantic==1){
                            const double a00=transform[0],a01=transform[4],a02=transform[8],a10=transform[1],a11=transform[5],a12=transform[9],a20=transform[2],a21=transform[6],a22=transform[10];
                            const double det=a00*(a11*a22-a12*a21)-a01*(a10*a22-a12*a20)+a02*(a10*a21-a11*a20);
                            if(std::abs(det)<1e-12)throw std::invalid_argument("Singular morph node transform");
                            values[v]={static_cast<float>(((a11*a22-a12*a21)*delta[0]+(a12*a20-a10*a22)*delta[1]+(a10*a21-a11*a20)*delta[2])/det),
                                static_cast<float>(((a02*a21-a01*a22)*delta[0]+(a00*a22-a02*a20)*delta[1]+(a01*a20-a00*a21)*delta[2])/det),
                                static_cast<float>(((a01*a12-a02*a11)*delta[0]+(a02*a10-a00*a12)*delta[1]+(a00*a11-a01*a10)*delta[2])/det)};
                        }else for(unsigned axis=0;axis<3;++axis)values[v][axis]=static_cast<float>(transform[axis]*delta[0]+transform[4+axis]*delta[1]+transform[8+axis]*delta[2]);
                        if(semantic>0){const float length=directionLengths[v][semantic-1];
                            if(length<1e-12F)throw std::invalid_argument("Degenerate morph direction basis");
                            for(auto& value:values[v])value/=length;
                        }
                    }
                }
            }
            range.targets.push_back(std::move(target));
        }
        asset.morphRanges.push_back(std::move(range));
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
    const std::vector<int>& rigidJoints,
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
                static_cast<std::uint32_t>(nodeIndex),
                rigidJoints.at(static_cast<std::size_t>(nodeIndex)),
                asset);
        }
    }
    for (const int child : node.children) {
        visitNode(model, child, worldTransform, fallbackMaterial, rigidJoints, asset);
    }
}

Matrix4 composeTransform(
    const std::array<float, 3>& translation,
    const std::array<float, 4>& rotation,
    const std::array<float, 3>& scale) {
    const double x = rotation[0];
    const double y = rotation[1];
    const double z = rotation[2];
    const double w = rotation[3];
    return {
        (1.0 - 2.0 * y * y - 2.0 * z * z) * scale[0],
        (2.0 * x * y + 2.0 * w * z) * scale[0],
        (2.0 * x * z - 2.0 * w * y) * scale[0],
        0.0,
        (2.0 * x * y - 2.0 * w * z) * scale[1],
        (1.0 - 2.0 * x * x - 2.0 * z * z) * scale[1],
        (2.0 * y * z + 2.0 * w * x) * scale[1],
        0.0,
        (2.0 * x * z + 2.0 * w * y) * scale[2],
        (2.0 * y * z - 2.0 * w * x) * scale[2],
        (1.0 - 2.0 * x * x - 2.0 * y * y) * scale[2],
        0.0,
        translation[0], translation[1], translation[2], 1.0,
    };
}

std::array<float, 4> normalizeQuaternion(
    std::array<float, 4> quaternion) {
    const float length = std::sqrt(
        quaternion[0] * quaternion[0]
        + quaternion[1] * quaternion[1]
        + quaternion[2] * quaternion[2]
        + quaternion[3] * quaternion[3]);
    if (length <= 1.0e-8F) {
        return {0.0F, 0.0F, 0.0F, 1.0F};
    }
    for (float& component : quaternion) {
        component /= length;
    }
    return quaternion;
}

std::array<float, 4> slerp(
    const std::array<float, 4>& start,
    std::array<float, 4> end,
    const float factor) {
    float cosine =
        start[0] * end[0] + start[1] * end[1]
        + start[2] * end[2] + start[3] * end[3];
    if (cosine < 0.0F) {
        cosine = -cosine;
        for (float& component : end) {
            component = -component;
        }
    }
    if (cosine > 0.9995F) {
        std::array<float, 4> result{};
        for (std::size_t component = 0; component < 4; ++component) {
            result[component] =
                start[component]
                + (end[component] - start[component]) * factor;
        }
        return normalizeQuaternion(result);
    }
    const float angle = std::acos(std::clamp(cosine, -1.0F, 1.0F));
    const float denominator = std::sin(angle);
    const float startWeight =
        std::sin((1.0F - factor) * angle) / denominator;
    const float endWeight = std::sin(factor * angle) / denominator;
    std::array<float, 4> result{};
    for (std::size_t component = 0; component < 4; ++component) {
        result[component] =
            start[component] * startWeight
            + end[component] * endWeight;
    }
    return normalizeQuaternion(result);
}

std::array<float, 4> sampleAnimationSampler(
    const AssetAnimationSampler& sampler,
    const float time,
    const bool rotation) {
    if (time <= sampler.inputTimes.front()) {
        return sampler.outputValues.front();
    }
    if (time >= sampler.inputTimes.back()) {
        return sampler.outputValues.back();
    }
    const auto upper = std::upper_bound(
        sampler.inputTimes.begin(),
        sampler.inputTimes.end(),
        time);
    const std::size_t endIndex =
        static_cast<std::size_t>(
            std::distance(sampler.inputTimes.begin(), upper));
    const std::size_t startIndex = endIndex - 1;
    if (sampler.interpolation == AssetAnimationInterpolation::Step) {
        return sampler.outputValues[startIndex];
    }
    const float interval =
        sampler.inputTimes[endIndex] - sampler.inputTimes[startIndex];
    const float factor = interval > 1.0e-8F
        ? (time - sampler.inputTimes[startIndex]) / interval
        : 0.0F;
    if (rotation) {
        return slerp(
            sampler.outputValues[startIndex],
            sampler.outputValues[endIndex],
            factor);
    }
    std::array<float, 4> result{};
    for (std::size_t component = 0; component < 4; ++component) {
        result[component] =
            sampler.outputValues[startIndex][component]
            + (sampler.outputValues[endIndex][component]
               - sampler.outputValues[startIndex][component])
                * factor;
    }
    return result;
}

}  // namespace

const char* assetMaterialClassName(const AssetMaterialClass value) {
    switch (value) {
        case AssetMaterialClass::Generic: return "generic";
        case AssetMaterialClass::Skin: return "skin";
        case AssetMaterialClass::Face: return "face";
        case AssetMaterialClass::Hair: return "hair";
        case AssetMaterialClass::Fabric: return "fabric";
        case AssetMaterialClass::Metal: return "metal";
        case AssetMaterialClass::Eye: return "eye";
        case AssetMaterialClass::Overlay: return "overlay";
        case AssetMaterialClass::Emissive: return "emissive";
        case AssetMaterialClass::Showcase: return "showcase";
    }
    return "generic";
}

namespace {
LoadedAsset importGltfModel(tinygltf::Model&,const AuthoredImageMips* authored=nullptr);
bool decodeGltfImage(tinygltf::Image* image,int imageIndex,std::string* error,std::string* warning,
    int requestedWidth,int requestedHeight,const unsigned char* bytes,int size,void* userData) {
    try {
        if(size<=0||size>64*1024*1024)throw std::invalid_argument("glTF image exceeds the source budget");
        const auto decoded=azurerender::decodeImageBytes({bytes,bytes+size});
        if((requestedWidth>0&&decoded.width!=static_cast<unsigned>(requestedWidth))||
           (requestedHeight>0&&decoded.height!=static_cast<unsigned>(requestedHeight)))throw std::invalid_argument("glTF image dimensions differ from the declaration");
        if(userData&&!decoded.mipLevels.empty())(*static_cast<AuthoredImageMips*>(userData))[imageIndex]={decoded.mipLevels,decoded.sourceCompression};
        auto material=azurerender::convertImageToRgba8(decoded);
        image->width=static_cast<int>(decoded.width);image->height=static_cast<int>(decoded.height);
        image->component=4;
        if(decoded.precision==azurerender::ImagePrecision::UInt8) {
            image->bits=8;image->pixel_type=TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;image->image=decoded.rgba8;
        }else if(decoded.precision==azurerender::ImagePrecision::UInt16) {
            image->bits=16;image->pixel_type=TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;image->image.resize(decoded.rgba16.size()*2);
            std::memcpy(image->image.data(),decoded.rgba16.data(),image->image.size());
        }else {
            image->bits=32;image->pixel_type=TINYGLTF_COMPONENT_TYPE_FLOAT;image->image.resize(decoded.rgba32.size()*4);
            std::memcpy(image->image.data(),decoded.rgba32.data(),image->image.size());
        }
        if(warning)for(const auto& loss:material.losses)*warning+="Image "+image->name+": "+loss+"\n";
        return true;
    }catch(const std::exception& e){if(error)*error+=e.what();return false;}
}
}
LoadedAsset loadGltfAsset(const std::string& path) {
    tinygltf::TinyGLTF loader;
    AuthoredImageMips authored;loader.SetImageLoader(decodeGltfImage,&authored);
    tinygltf::Model model;
    std::string warning;
    std::string error;
    const auto filePath = std::filesystem::u8path(path);
    auto extension=filePath.extension().u8string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const bool loaded = extension == ".glb"
        ? loader.LoadBinaryFromFile(&model, &error, &warning, path)
        : loader.LoadASCIIFromFile(&model, &error, &warning, path);
    if (!warning.empty()) {
        azurerender::RuntimeDiagnostics::instance().warning(
            "asset", "[glTF] " + warning);
    }
    if (!loaded) {
        throw std::runtime_error("Unable to load glTF asset: " + error);
    }
    auto asset=importGltfModel(model,&authored);
    for(std::size_t index=0;index<model.materials.size();++index) {
        const auto& material=model.materials[index];
        const int textures[]={material.pbrMetallicRoughness.baseColorTexture.index,material.normalTexture.index,material.pbrMetallicRoughness.metallicRoughnessTexture.index,material.emissiveTexture.index,material.occlusionTexture.index};
        for(std::size_t slot=0;slot<5;++slot) {
            if(textures[slot]<0)continue;
            const auto image=model.textures.at(static_cast<std::size_t>(textures[slot])).source;
            if(image<0)continue;const auto& uri=model.images.at(static_cast<std::size_t>(image)).uri;
            if(uri.empty()||uri.rfind("data:",0)==0)continue;
            const auto sidecar=filePath.parent_path()/std::filesystem::u8path(uri+".azmeta");
            if(!std::filesystem::exists(sidecar))continue;
            std::ifstream input(sidecar);nlohmann::json metadata;input>>metadata;
            const auto settings=metadata.value("settings",nlohmann::json::object());
            if(settings.contains("texture"))asset.materials[index].textureSettings[slot]=azurerender::TextureImportSettings::parse(settings.at("texture"),asset.materials[index].textureSettings[slot]);
        }
    }
    return asset;
}
LoadedAsset loadEmbeddedGltfAsset(const std::string& bytes,bool binary){
    if(bytes.empty()||bytes.size()>128*1024*1024)throw std::invalid_argument("Embedded glTF source exceeds byte budget");
    tinygltf::TinyGLTF loader;AuthoredImageMips authored;loader.SetImageLoader(decodeGltfImage,&authored);tinygltf::Model model;std::string error,warning;bool external=false;
    tinygltf::FsCallbacks fs;
    fs.FileExists=[&](const std::string&,void*){external=true;return false;};
    fs.ExpandFilePath=[](const std::string& path,void*){return path;};
    fs.ReadWholeFile=[&](std::vector<unsigned char>*,std::string*,const std::string&,void*){external=true;return false;};
    fs.WriteWholeFile=[&](std::string*,const std::string&,const std::vector<unsigned char>&,void*){external=true;return false;};
    fs.GetFileSizeInBytes=[&](std::size_t*,std::string*,const std::string&,void*){external=true;return false;};fs.user_data=nullptr;
    if(!loader.SetFsCallbacks(fs,&error))throw std::runtime_error("Cannot establish embedded asset filesystem boundary: "+error);
    const auto size=static_cast<unsigned>(bytes.size());
    const bool loaded=binary?loader.LoadBinaryFromMemory(&model,&error,&warning,reinterpret_cast<const unsigned char*>(bytes.data()),size)
        :loader.LoadASCIIFromString(&model,&error,&warning,bytes.data(),size,"");
    if(external)throw std::invalid_argument("Embedded model candidates require embedded buffers and images");
    if(!loaded)throw std::invalid_argument("Invalid embedded model candidate: "+error);
    return importGltfModel(model,&authored);
}
namespace {
LoadedAsset importGltfModel(tinygltf::Model& model,const AuthoredImageMips* authored){
    normalizeGltfAccessors(model);
    LoadedAsset asset;
    asset.materials.reserve(model.materials.size() + 1);
    for (const auto& material : model.materials) {
        asset.materials.push_back(loadMaterial(model, &material,authored));
    }
    const std::uint32_t fallbackMaterial =
        static_cast<std::uint32_t>(asset.materials.size());
    asset.materials.push_back(loadMaterial(model, nullptr,authored));
    loadNodes(model, asset);
    const std::vector<Matrix4> nodeWorldTransforms =
        calculateNodeWorldTransforms(model);
    asset.nodeWorldMatrices.resize(nodeWorldTransforms.size());
    for (std::size_t index = 0; index < nodeWorldTransforms.size(); ++index) {
        std::transform(
            nodeWorldTransforms[index].begin(),
            nodeWorldTransforms[index].end(),
            asset.nodeWorldMatrices[index].begin(),
            [](const double value) { return static_cast<float>(value); });
    }
    loadJointMatrices(model, nodeWorldTransforms, asset);
    const auto rigidJoints=loadRigidNodeJoints(model,nodeWorldTransforms,asset);
    loadAnimations(model, asset);

    if (!model.scenes.empty()) {
        const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        const auto& scene = model.scenes.at(static_cast<std::size_t>(sceneIndex));
        for (const int node : scene.nodes) {
            visitNode(model, node, identityMatrix(), fallbackMaterial, rigidJoints, asset);
        }
    } else {
        for (std::size_t index = 0; index < model.nodes.size(); ++index) {
            if (model.nodes[index].mesh >= 0) {
                visitNode(
                    model,
                    static_cast<int>(index),
                    identityMatrix(),
                    fallbackMaterial,
                    rigidJoints,
                    asset);
            }
        }
    }

    if (asset.primitives.empty()) {
        throw std::runtime_error("glTF scene contains no triangle mesh primitive");
    }
    azurerender::prepareBrowRegions(asset);
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
}

namespace {
void resolvePose(const LoadedAsset& asset, AssetPose& pose) {
    std::vector<Matrix4> worldTransforms(
        asset.nodes.size(),
        identityMatrix());
    std::vector<bool> calculated(asset.nodes.size(), false);
    std::function<const Matrix4&(std::size_t)> calculate =
        [&](const std::size_t index) -> const Matrix4& {
        if (calculated[index]) {
            return worldTransforms[index];
        }
        Matrix4 local{};
        if (asset.nodes[index].usesMatrix) {
            std::transform(
                asset.nodes[index].localMatrix.begin(),
                asset.nodes[index].localMatrix.end(),
                local.begin(),
                [](const float value) {
                    return static_cast<double>(value);
                });
        } else {
            local = composeTransform(
                pose.local[index].translation,
                pose.local[index].rotation,
                pose.local[index].scale);
        }
        const std::int32_t parent = asset.nodes[index].parent;
        worldTransforms[index] = parent >= 0
            ? multiply(
                  calculate(static_cast<std::size_t>(parent)),
                  local)
            : local;
        calculated[index] = true;
        return worldTransforms[index];
    };
    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        calculate(index);
    }

    pose.nodeWorldMatrices.resize(worldTransforms.size());
    for (std::size_t index = 0; index < worldTransforms.size(); ++index) {
        std::transform(
            worldTransforms[index].begin(),
            worldTransforms[index].end(),
            pose.nodeWorldMatrices[index].begin(),
            [](const double value) { return static_cast<float>(value); });
    }

    if (!asset.hasSkin) {
        pose.jointMatrices = asset.jointMatrices;
        return;
    }
    pose.jointMatrices.resize(asset.jointNodes.size());
    for (std::size_t joint = 0;
         joint < asset.jointNodes.size();
         ++joint) {
        Matrix4 inverseBind{};
        std::transform(
            asset.inverseBindMatrices[joint].begin(),
            asset.inverseBindMatrices[joint].end(),
            inverseBind.begin(),
            [](const float value) {
                return static_cast<double>(value);
            });
        const Matrix4 matrix = multiply(
            worldTransforms.at(asset.jointNodes[joint]),
            inverseBind);
        std::transform(
            matrix.begin(),
            matrix.end(),
            pose.jointMatrices[joint].begin(),
            [](const double value) {
                return static_cast<float>(value);
            });
    }
}
}
void resolveAssetPose(const LoadedAsset& asset, AssetPose& pose) {
    if(pose.local.size()!=asset.nodes.size())throw std::invalid_argument("Pose skeleton mismatch");
    resolvePose(asset,pose);
}
AssetPose bindAnimationPose(const LoadedAsset& asset) {
    AssetPose pose;
    for (const auto& node : asset.nodes) pose.local.push_back({node.translation, node.rotation, node.scale});
    pose.morphWeights=asset.defaultMorphWeights;
    resolvePose(asset, pose);
    return pose;
}
AssetPose sampleAnimationPose(const LoadedAsset& asset, std::size_t animationIndex, float time, bool loop) {
    if (animationIndex >= asset.animations.size()) throw std::runtime_error("Animation index is out of range");
    if (!std::isfinite(time)) throw std::invalid_argument("Animation time must be finite");
    auto pose = bindAnimationPose(asset);
    const auto& animation = asset.animations[animationIndex];
    const float duration = animation.endTime - animation.startTime;
    const float sampleTime = duration > 1e-8F
        ? animation.startTime + (loop ? std::fmod(std::max(time, 0.0F), duration) : std::clamp(time, 0.0F, duration))
        : animation.startTime;
    for (const auto& channel : animation.channels) {
        const auto& sampler = animation.samplers.at(channel.samplerIndex);
        if(channel.path==AssetAnimationPath::Weights){pose.morphWeights.at(channel.nodeIndex)=sampleCurve(sampler.curve,sampleTime);continue;}
        auto value = std::array<float,4>{};
        if(sampler.curve.width){const auto sampled=sampleCurve(sampler.curve,sampleTime);std::copy(sampled.begin(),sampled.end(),value.begin());}
        else value=sampleAnimationSampler(sampler,sampleTime,channel.path==AssetAnimationPath::Rotation);
        auto& node = pose.local.at(channel.nodeIndex);
        if (channel.path == AssetAnimationPath::Rotation) node.rotation = normalizeQuaternion(value);
        else if (channel.path == AssetAnimationPath::Translation) std::copy_n(value.begin(), 3, node.translation.begin());
        else std::copy_n(value.begin(), 3, node.scale.begin());
    }
    resolvePose(asset, pose);
    return pose;
}
AssetPose blendAnimationPoses(const LoadedAsset& asset, const AssetPose& from, const AssetPose& to, float weight) {
    if (!std::isfinite(weight) || weight < 0 || weight > 1) throw std::invalid_argument("Pose blend weight must be in [0,1]");
    if (from.local.size() != asset.nodes.size() || to.local.size() != asset.nodes.size()) throw std::invalid_argument("Pose skeleton mismatch");
    AssetPose result;
    result.local.resize(asset.nodes.size());
    for (std::size_t i = 0; i < result.local.size(); ++i) {
        auto& node = result.local[i];
        for (unsigned axis = 0; axis < 3; ++axis) {
            node.translation[axis] = from.local[i].translation[axis] * (1-weight) + to.local[i].translation[axis] * weight;
            node.scale[axis] = from.local[i].scale[axis] * (1-weight) + to.local[i].scale[axis] * weight;
        }
        node.rotation = slerp(from.local[i].rotation, to.local[i].rotation, weight);
    }
    result.morphWeights=from.morphWeights;
    if(result.morphWeights.size()!=to.morphWeights.size())throw std::invalid_argument("Morph pose node count mismatch");
    for(std::size_t n=0;n<result.morphWeights.size();++n){
        if(result.morphWeights[n].size()!=to.morphWeights[n].size())throw std::invalid_argument("Morph pose width mismatch");
        for(std::size_t t=0;t<result.morphWeights[n].size();++t)result.morphWeights[n][t]=from.morphWeights[n][t]*(1-weight)+to.morphWeights[n][t]*weight;
    }
    resolvePose(asset, result);
    return result;
}
void sampleAnimation(const LoadedAsset& asset, std::size_t animationIndex, float time,
                     std::vector<std::array<float,16>>& jointMatrices) {
    jointMatrices = sampleAnimationPose(asset, animationIndex, time).jointMatrices;
}

AssetImage loadAssetImage(const std::string& path){
    const auto image=azurerender::decodeImageFile(std::filesystem::u8path(path));
    auto converted=azurerender::convertImageToRgba8(image);
    AssetImage result;result.width=image.width;result.height=image.height;
    result.pixels=std::move(converted.pixels);result.losses=std::move(converted.losses);result.mipLevels=image.mipLevels;result.sourceCompression=image.sourceCompression;return result;
}
