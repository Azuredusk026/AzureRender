#pragma once
#include "assets/GltfLoader.hpp"
#include "render/RenderContext.hpp"
#include <map>
namespace azurerender::tracing {
using Vec4=std::array<float,4>;
struct Triangle {Vec4 p0,e1,e2,uv01,uv2material;};
struct BvhNode {Vec4 minimum,maximum;std::array<std::uint32_t,4> links{};};
struct Material {std::array<std::uint32_t,4> albedo{},surface{},emission{};Vec4 properties{};};
struct Light {Vec4 position{},radiance{};};
struct Geometry {std::vector<Triangle> triangles;std::vector<BvhNode> nodes;std::vector<Light> lights;};
struct Mesh {LoadedAsset asset;std::uint32_t materialBase=0;};
using Meshes=std::map<std::string,Mesh>;
void buildBvh(Geometry& geometry);
Geometry buildGeometry(const scene::SceneDescription&,const Meshes&,const std::vector<NodeAnimationFrame>&);
std::uint64_t sceneFingerprint(const scene::SceneDescription&,const std::vector<NodeAnimationFrame>&);
std::array<std::uint32_t,4> appendTexture(std::vector<std::uint32_t>& pixels,const std::vector<std::uint8_t>& rgba,unsigned width,unsigned height,bool srgb);
}
