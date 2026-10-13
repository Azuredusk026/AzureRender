#pragma once
#include "extensions/ExtensionRegistry.hpp"
namespace azurerender {
class BuiltinRenderFeature final:public IRenderFeature {
public:
    BuiltinRenderFeature(std::string id,std::vector<std::string> capabilities,std::vector<std::string> resources)
        :id_(std::move(id)),capabilities_(std::move(capabilities)),resources_(std::move(resources)){}
    std::string_view name() const noexcept override{return id_;}
    std::vector<std::string> capabilities() const override{return capabilities_;}
    std::vector<std::string> resourceDependencies() const override{return resources_;}
private:
    std::string id_;std::vector<std::string> capabilities_,resources_;
};
inline RenderFeatureRegistry builtinRenderFeatures(){
    RenderFeatureRegistry registry;
    registry.registerFactory({"face-sdf",1,{"material.face","head-relative-light"},{}},[]{return std::make_unique<BuiltinRenderFeature>("face-sdf",std::vector<std::string>{"material.face","head-relative-light"},std::vector<std::string>{"material.faceSdf.texture","material.faceSdf.headNode"});});
    registry.registerFactory({"showcase-platform",1,{"presentation.platform"},{}},[]{return std::make_unique<BuiltinRenderFeature>("showcase-platform",std::vector<std::string>{"presentation.platform"},std::vector<std::string>{"generated.platform-mesh"});});
    return registry;
}
}
