#include "ViewportOverlayRegistry.hpp"
#include "editor/GameplayDebugGeometry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/physics/PhysicsDescriptors.hpp"
#include "assets/GltfLoader.hpp"
#include <limits>
namespace azurerender {
namespace {
ecs::TransformComponent transform(const SceneNode& node){return {node.translation,node.rotation,node.scale};}
template<class T>T decode(const char* type,const nlohmann::json& fields){T result;auto envelope=runtimeComponentRegistry().defaults(type);envelope["data"].update(fields);runtimeComponentRegistry().metadata().decode(type,&result,envelope);return result;}
}
void registerPhysicsOverlayProviders(ViewportOverlayRegistry& registry){
    // Per-provider bounds cache follows immutable asset fingerprints.
    auto bounds=std::make_shared<std::map<std::string,std::pair<std::array<float,3>,std::array<float,3>>>>();
    registry.add("collision","Collision shapes",[bounds](const auto& request){
        std::vector<ViewportOverlayLine> lines;const auto& scene=request.scene?*request.scene:request.context.scene();
        for(const auto& node:scene.nodes){
            const auto fields=request.context.componentData(node.id,"azure.rigid-body"),character=request.context.componentData(node.id,"azure.character");
            auto placement=transform(node);std::vector<DebugLine> shape;
            if(!fields.is_null()){
                auto body=decode<game::RigidBody>("azure.rigid-body",fields);
                if(body.shape=="mesh"&&request.context.isProject()){
                    const auto path=request.context.assets().resolveReference(body.mesh);const auto key=path.u8string()+":"+std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
                    if(!bounds->count(key)){const auto asset=loadGltfAsset(path.u8string());(*bounds)[key]={asset.boundsMin,asset.boundsMax};if(bounds->size()>128)bounds->erase(bounds->begin());}
                    const auto [minimum,maximum]=bounds->at(key);const auto r=JPH::Quat::sEulerAngles(JPH::Vec3(placement.rotation[0],placement.rotation[1],placement.rotation[2])*.017453292519943295F);
                    JPH::Vec3 center;for(unsigned i=0;i<3;++i){body.halfExtent[i]=(maximum[i]-minimum[i])*.5F;center.SetComponent(i,(maximum[i]+minimum[i])*.5F*placement.scale[i]);}
                    center=r*center;placement.translation[0]+=center.GetX();placement.translation[1]+=center.GetY();placement.translation[2]+=center.GetZ();
                }
                shape=debugRigidBody(placement,body);
            }else if(!character.is_null())shape=debugCapsule(placement,decode<game::Character>("azure.character",character));
            for(const auto& edge:shape)lines.push_back({edge.from,edge.to,{.3F,.9F,.5F,.85F},node.id});
        }return lines;
    });
    registry.add("hinge","Hinge constraints",[](const auto& request){
        std::vector<ViewportOverlayLine> lines;const auto& scene=request.scene?*request.scene:request.context.scene();
        for(const auto& node:scene.nodes){const auto fields=request.context.componentData(node.id,"azure.hinge");if(fields.is_null())continue;
            const auto hinge=decode<game::Hinge>("azure.hinge",fields);validateHinge(hinge,false);
            auto anchor=[&](const std::string& id,const auto& point)->std::optional<std::array<float,3>>{
                for(const auto& endpoint:scene.nodes)if(endpoint.id==id){const auto t=transform(endpoint);const auto r=JPH::Quat::sEulerAngles(JPH::Vec3(t.rotation[0],t.rotation[1],t.rotation[2])*.017453292519943295F);const auto p=r*JPH::Vec3(point[0]*t.scale[0],point[1]*t.scale[1],point[2]*t.scale[2]);return std::array<float,3>{t.translation[0]+p.GetX(),t.translation[1]+p.GetY(),t.translation[2]+p.GetZ()};}return {};};
            const auto a=anchor(hinge.bodyA,hinge.anchorA),b=anchor(hinge.bodyB,hinge.anchorB);if(!a||!b)continue;
            lines.push_back({*a,*b,{1,.6F,.2F,1},node.id});
            const auto axis=JPH::Vec3(hinge.axis[0],hinge.axis[1],hinge.axis[2]).Normalized();const auto normal=JPH::Vec3(hinge.normal[0],hinge.normal[1],hinge.normal[2]).Normalized();const auto tangent=axis.Cross(normal);
            auto point=[&](float angle){const auto p=normal*std::cos(angle)*.5F+tangent*std::sin(angle)*.5F;return std::array<float,3>{(*a)[0]+p.GetX(),(*a)[1]+p.GetY(),(*a)[2]+p.GetZ()};};
            auto end=*a;for(unsigned i=0;i<3;++i)end[i]+=hinge.axis[i];lines.push_back({*a,end,{1,.8F,.3F,1},node.id});
            for(unsigned i=0;i<24;++i){const float lo=hinge.minimumAngle*.017453292519943295F,span=(hinge.maximumAngle-hinge.minimumAngle)*.017453292519943295F;lines.push_back({point(lo+span*static_cast<float>(i)/24),point(lo+span*static_cast<float>(i+1)/24),{1,.6F,.2F,.85F},node.id});}
        }return lines;
    });
}
}
