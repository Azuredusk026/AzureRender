#include "PhysicsShapeFactory.hpp"
#include "runtime/AssetDatabase.hpp"
#include "assets/GltfLoader.hpp"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <algorithm>
namespace azurerender {
JPH::RefConst<JPH::Shape> createPhysicsShape(const game::RigidBody& body,const std::array<float,3>& scale,const CollisionMeshResolver& resolve){
    validateRigidBody(body,scale);
    if(body.shape=="box"){
        JPH::Vec3 extent{body.halfExtent[0]*std::abs(scale[0]),body.halfExtent[1]*std::abs(scale[1]),body.halfExtent[2]*std::abs(scale[2])};
        return new JPH::BoxShape(extent,std::min(.05F,extent.ReduceMin()*.5F));
    }
    if(body.shape=="sphere")return new JPH::SphereShape(body.radius*std::abs(scale[0]));
    if(body.shape=="capsule")return new JPH::CapsuleShape(body.halfHeight*std::abs(scale[1]),body.radius*std::abs(scale[0]));
    if(!resolve)throw std::invalid_argument("Static mesh collision requires an asset resolver");
    const auto source=resolve(body.mesh);if(source.empty()||source.size()>1000000)throw std::invalid_argument("Collision mesh triangle count outside budget");
    JPH::TriangleList triangles;triangles.reserve(source.size());
    for(auto t:source){for(auto& vertex:t)for(unsigned i=0;i<3;++i){vertex[i]*=scale[i];if(!std::isfinite(vertex[i]))throw std::invalid_argument("Collision mesh must be finite");}
        if(scale[0]*scale[1]*scale[2]<0)std::swap(t[1],t[2]);
        auto v=[](const auto& p){return JPH::Vec3(p[0],p[1],p[2]);};
        if((v(t[1])-v(t[0])).Cross(v(t[2])-v(t[0])).LengthSq()<1e-12F)throw std::invalid_argument("Degenerate collision triangle");
        triangles.emplace_back(v(t[0]),v(t[1]),v(t[2]));
    }
    auto result=JPH::MeshShapeSettings(triangles).Create();
    if(result.HasError())throw std::invalid_argument(result.GetError().c_str());return result.Get();
}
CollisionMeshResolver collisionMeshAssets(const AssetDatabase& assets){
    return [&assets](const std::string& reference){
        const auto path=assets.resolveReference(reference);
        const auto asset=loadGltfAsset(path.u8string());
        if(asset.hasSkin||asset.morphTargetCount)throw std::invalid_argument("Static collision mesh requires rigid geometry");
        if(asset.indices.size()%3)throw std::invalid_argument("Collision model requires triangle indices");
        CollisionMesh result;result.reserve(asset.indices.size()/3);
        for(std::size_t i=0;i<asset.indices.size();i+=3)result.push_back({asset.vertices.at(asset.indices[i]).position,asset.vertices.at(asset.indices[i+1]).position,asset.vertices.at(asset.indices[i+2]).position});
        return result;
    };
}
}
