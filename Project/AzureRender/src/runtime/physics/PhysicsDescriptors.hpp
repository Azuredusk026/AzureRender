#pragma once
#include "runtime/GameComponents.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>
#include <functional>
namespace azurerender {
using CollisionTriangle=std::array<std::array<float,3>,3>;
using CollisionMesh=std::vector<CollisionTriangle>;
using CollisionMeshResolver=std::function<CollisionMesh(const std::string&)>;
inline void validateRigidBody(const game::RigidBody& body,const std::array<float,3>& scale={1,1,1}){
    for(unsigned i=0;i<3;++i)if(!std::isfinite(scale[i])||std::abs(scale[i])<.001F||!std::isfinite(body.halfExtent[i])||body.halfExtent[i]<=0)throw std::invalid_argument("Collision extent and absolute scale must be positive and finite");
    if(!std::isfinite(body.radius)||body.radius<=0||!std::isfinite(body.halfHeight)||body.halfHeight<=0)throw std::invalid_argument("Collision radius and half height must be positive and finite");
    if(body.shape!="box"&&body.shape!="sphere"&&body.shape!="capsule"&&body.shape!="mesh")throw std::invalid_argument("Unknown collision shape");
    const auto same=[](float a,float b){return std::abs(std::abs(a)-std::abs(b))<1e-5F;};
    if(body.shape=="sphere"&&(!same(scale[0],scale[1])||!same(scale[0],scale[2])))throw std::invalid_argument("Sphere requires uniform absolute scale");
    if(body.shape=="capsule"&&!same(scale[0],scale[2]))throw std::invalid_argument("Capsule requires equal absolute X/Z scale");
    if(body.shape=="mesh"&&(body.dynamic||body.mesh.empty()||body.trigger))throw std::invalid_argument("Mesh collision requires a static nonsensor body and model reference");
}
inline void validateHinge(const game::Hinge& hinge,bool requireEndpoints=true){
    if(requireEndpoints&&(hinge.bodyA.empty()||hinge.bodyB.empty()||hinge.bodyA==hinge.bodyB))throw std::invalid_argument("Hinge requires two distinct body references");
    float axis=0,normal=0,dot=0;
    for(unsigned i=0;i<3;++i){
        for(float value:{hinge.axis[i],hinge.normal[i],hinge.anchorA[i],hinge.anchorB[i]})if(!std::isfinite(value))throw std::invalid_argument("Hinge vectors must be finite");
        axis+=hinge.axis[i]*hinge.axis[i];normal+=hinge.normal[i]*hinge.normal[i];dot+=hinge.axis[i]*hinge.normal[i];
    }
    if(axis<1e-6F||normal<1e-6F||std::abs(dot)>1e-4F*std::sqrt(axis*normal))throw std::invalid_argument("Hinge axis and normal must be nonzero and perpendicular");
    if(!std::isfinite(hinge.minimumAngle)||!std::isfinite(hinge.maximumAngle)||hinge.minimumAngle< -180||hinge.minimumAngle>0||hinge.maximumAngle<0||hinge.maximumAngle>180)throw std::invalid_argument("Hinge limits must bracket zero within 180 degrees");
}
}
