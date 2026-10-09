#include "ViewportOverlayRegistry.hpp"
#include "scene/SceneDescription.hpp"
#include "render/RenderMath.hpp"
#include <algorithm>
namespace azurerender {
void ViewportOverlayRegistry::add(std::string id,std::string label,Provider provider){
    if(id.empty()||label.empty()||!provider||!providers_.emplace(std::move(id),Entry{std::move(label),std::move(provider)}).second)throw std::invalid_argument("Invalid or duplicate viewport helper provider");
}
nlohmann::json ViewportOverlayRegistry::describe() const{auto result=nlohmann::json::array();for(const auto& [id,entry]:providers_)result.push_back({{"category",id},{"label",entry.label}});return result;}
std::vector<ViewportOverlayLine> ViewportOverlayRegistry::collect(const ViewportOverlayRequest& request,const std::map<std::string,std::string>& policies,const std::vector<std::string>& selected) const{
    std::vector<ViewportOverlayLine> result;
    for(const auto& [id,entry]:providers_){const auto found=policies.find(id);const auto policy=found==policies.end()?"hidden":found->second;if(policy=="hidden")continue;
        if(policy!="selected"&&policy!="all")throw std::invalid_argument("Unknown viewport helper visibility policy");
        for(auto& line:entry.provider(request)){if(policy=="selected"&&(line.node.empty()||std::find(selected.begin(),selected.end(),line.node)==selected.end()))continue;
            if(result.size()>=4096)throw std::runtime_error("Viewport helper line budget exceeded");
            for(const auto& vector:{line.from,line.to})for(float value:vector)if(!std::isfinite(value))throw std::invalid_argument("Helper geometry must be finite");
            for(float value:line.color)if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Helper color must be within zero and one");result.push_back(std::move(line));
        }
    }return result;
}
ViewportOverlayRegistry ViewportOverlayRegistry::builtins(){
    ViewportOverlayRegistry result;
    result.add("skeleton","Animation skeleton",[](const auto& request){return request.skeleton?*request.skeleton:std::vector<ViewportOverlayLine>{};});
    result.add("grid","Ground grid",[](const auto& request){
        std::vector<ViewportOverlayLine> lines;const auto distance=internal::vectorLength(internal::subtract(request.view.position,request.view.target));
        const float scale=request.view.projection==CameraProjectionKind::Orthographic?request.view.orthoHeight:distance;
        const auto step=std::pow(10.F,std::floor(std::log10(std::max(scale*.15F,.001F))));
        const float x=std::floor(request.view.target[0]/step)*step,z=std::floor(request.view.target[2]/step)*step;
        for(int i=-20;i<=20;++i){const float coordinate=static_cast<float>(i)*step;const float alpha=i%5==0?.38F:.18F;
            lines.push_back({{x+coordinate,0,z-20*step},{x+coordinate,0,z+20*step},{.58F,.63F,.70F,alpha},{}});
            lines.push_back({{x-20*step,0,z+coordinate},{x+20*step,0,z+coordinate},{.58F,.63F,.70F,alpha},{}});
        }return lines;
    });
    result.add("light","Light ranges",[](const auto& request){
        std::vector<ViewportOverlayLine> lines;const auto scene=request.context.scene().renderDescription();const auto world=scene::resolveNodeWorldTransforms(scene);
        for(const auto& light:request.context.scene().lights){const auto node=std::find_if(scene.nodes.begin(),scene.nodes.end(),[&](const auto& n){return n.id==light.nodeId;});if(node==scene.nodes.end()||!light.enabled)continue;
            const auto center=internal::transformPosition(world[static_cast<std::size_t>(node-scene.nodes.begin())],{0,0,0});
            for(unsigned plane=0;plane<3;++plane)for(unsigned segment=0;segment<32;++segment){auto point=[&](unsigned index){const float angle=static_cast<float>(index)*6.283185307F/32;auto p=center;p[plane]+=std::cos(angle)*light.radius;p[(plane+1)%3]+=std::sin(angle)*light.radius;return p;};lines.push_back({point(segment),point(segment+1),{1,.78F,.24F,.8F},light.nodeId});}
        }return lines;
    });
    result.add("camera","Camera rigs",[](const auto& request){
        std::vector<ViewportOverlayLine> lines;const auto scene=request.context.scene().renderDescription();const auto world=scene::resolveNodeWorldTransforms(scene);
        for(std::size_t i=0;i<scene.nodes.size();++i){const auto& node=scene.nodes[i];if(request.context.componentData(node.id,"azure.third-person-camera").is_null())continue;
            std::array<internal::Vector3,4> corners;for(unsigned c=0;c<4;++c)corners[c]=internal::transformPosition(world[i],{c&1?.5F:-.5F,c&2?.3F:-.3F,-1});
            const auto center=internal::transformPosition(world[i],{0,0,0});for(unsigned c=0;c<4;++c){lines.push_back({center,corners[c],{.25F,.75F,1,.85F},node.id});for(unsigned axis=0;axis<2;++axis)if(!(c&(1U<<axis)))lines.push_back({corners[c],corners[c|(1U<<axis)],{.25F,.75F,1,.85F},node.id});}
        }return lines;
    });registerPhysicsOverlayProviders(result);return result;
}
}
