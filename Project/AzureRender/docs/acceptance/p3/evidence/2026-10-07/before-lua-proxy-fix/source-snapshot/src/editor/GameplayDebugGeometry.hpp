#pragma once
#include "ecs/Components.hpp"
#include "runtime/GameComponents.hpp"
#include <vector>
#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <cmath>
namespace azurerender {
struct DebugLine { std::array<float,3> from,to; };
inline std::vector<DebugLine> debugBox(const ecs::TransformComponent& transform,const game::RigidBody& body){
    std::array<std::array<float,3>,8> points{};
    const auto rotation=JPH::Quat::sEulerAngles(JPH::Vec3(transform.rotation[0],transform.rotation[1],transform.rotation[2])*.017453292519943295F);
    for(unsigned corner=0;corner<8;++corner){std::array<float,3> local{};
        for(unsigned axis=0;axis<3;++axis)local[axis]=(corner&(1U<<axis)?1.0F:-1.0F)*body.halfExtent[axis]*std::abs(transform.scale[axis]);
        const auto rotated=rotation*JPH::Vec3(local[0],local[1],local[2]);
        points[corner]={transform.translation[0]+rotated.GetX(),transform.translation[1]+rotated.GetY(),transform.translation[2]+rotated.GetZ()};
    }
    std::vector<DebugLine> lines;
    for(unsigned corner=0;corner<8;++corner)for(unsigned axis=0;axis<3;++axis)if(!(corner&(1U<<axis)))lines.push_back({points[corner],points[corner|(1U<<axis)]});
    return lines;
}
inline std::vector<DebugLine> debugCapsule(const ecs::TransformComponent& transform,const game::Character& character){
    std::vector<DebugLine> lines;auto center=transform.translation;center[1]+=character.centerOffset;
    constexpr unsigned segments=24;constexpr float pi=3.14159265358979323846F;
    for(unsigned plane=0;plane<3;++plane){
        auto point=[&](float angle,float end){auto result=center;
            if(plane==0){result[0]+=character.radius*std::cos(angle);result[2]+=character.radius*std::sin(angle);result[1]+=end;}
            else {result[plane==1?0:2]+=character.radius*std::cos(angle);const float sine=std::sin(angle);result[1]+=character.radius*sine+(sine>=0?character.halfHeight:-character.halfHeight);}
            return result;
        };
        for(unsigned segment=0;segment<segments;++segment){const float a=2*pi*segment/segments,b=2*pi*(segment+1)/segments;
            lines.push_back({point(a,character.halfHeight),point(b,character.halfHeight)});
            if(plane==0)lines.push_back({point(a,-character.halfHeight),point(b,-character.halfHeight)});
        }
        if(plane>0)for(float sign:{-1.0F,1.0F}){auto bottom=center,top=center;bottom[1]-=character.halfHeight;top[1]+=character.halfHeight;bottom[plane==1?0:2]+=sign*character.radius;top[plane==1?0:2]+=sign*character.radius;lines.push_back({bottom,top});}
    }return lines;
}
}
