#pragma once
#include "assets/GltfLoader.hpp"
#include <nlohmann/json.hpp>

struct ExpressionBoneDelta {
    std::string path;
    std::array<float,3> translation{};
    std::array<float,4> rotation{0,0,0,1};
};
struct ExpressionMorphValue {std::string nodePath,targetId;float weight=0;};
struct ExpressionPose {
    std::uint32_t schemaVersion=1;
    std::string id,skeletonFingerprint;
    std::vector<ExpressionBoneDelta> bones;
    std::vector<ExpressionMorphValue> morphs;
};
[[nodiscard]] std::string expressionSkeletonFingerprint(const LoadedAsset&);
[[nodiscard]] ExpressionPose expressionPoseFromJson(const nlohmann::json&);
[[nodiscard]] nlohmann::json expressionPoseToJson(const ExpressionPose&);
[[nodiscard]] std::vector<float> morphOverridesFromJson(const std::string&);
[[nodiscard]] AssetPose applyExpressionPose(const LoadedAsset&,const AssetPose&,const ExpressionPose&,float weight=1);
