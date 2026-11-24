#pragma once
#include "assets/GltfLoader.hpp"
#include <nlohmann/json.hpp>

struct ExpressionBoneDelta {
    std::string path;
    std::array<float,3> translation{};
    std::array<float,4> rotation{0,0,0,1};
};
struct ExpressionHumanoidRotation {
    std::string path;
    std::array<float,4> zero{0,0,0,1};
    std::array<std::array<float,3>,3> axes{};
    std::array<std::array<float,2>,3> rates{};
    std::array<float,3> offset{};
};
struct ExpressionMorphValue {
    std::string nodePath,targetId;
    float weight=0;
    bool additive=false;
};
struct ExpressionPose {
    std::uint32_t schemaVersion=1;
    std::string id,skeletonFingerprint;
    std::vector<ExpressionBoneDelta> bones;
    std::vector<ExpressionMorphValue> morphs;
    std::vector<ExpressionHumanoidRotation> humanoidRotations;
};
[[nodiscard]] std::string expressionSkeletonFingerprint(const LoadedAsset&);
[[nodiscard]] ExpressionPose expressionPoseFromJson(const nlohmann::json&);
[[nodiscard]] nlohmann::json expressionPoseToJson(const ExpressionPose&);
[[nodiscard]] std::vector<float> morphOverridesFromJson(const std::string&);
[[nodiscard]] AssetPose applyExpressionPose(const LoadedAsset&,const AssetPose&,const ExpressionPose&,float weight=1);
