#include "render/pathtracing/TraceScene.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 static_assert(sizeof(tracing::Triangle)==176);static_assert(sizeof(tracing::BvhNode)==48);static_assert(sizeof(tracing::Material)==80);
 tracing::Geometry geometry;
 for(unsigned i=0;i<17;++i)geometry.triangles.push_back({{static_cast<float>(i),0,0,0},{1,0,0,0},{0,1,0,0},{},{}});
 tracing::buildBvh(geometry);unsigned count=0;
 for(const auto& node:geometry.nodes){if(node.links[1]){count+=node.links[1];for(unsigned j=0;j<node.links[1];++j){const auto& t=geometry.triangles.at(node.links[0]+j);for(unsigned a=0;a<3;++a){require(t.p0[a]>=node.minimum[a]&&t.p0[a]<=node.maximum[a],"Leaf must contain triangle origin");require(t.p0[a]+t.e1[a]<=node.maximum[a]&&t.p0[a]+t.e2[a]<=node.maximum[a],"Leaf must contain endpoints");}}}else{require(node.links[0]<geometry.nodes.size()&&node.links[2]<geometry.nodes.size(),"Valid child indices");}}
 require(count==17,"Every triangle belongs to exactly one leaf");
 tracing::Mesh mesh;mesh.asset.vertices.resize(3);mesh.asset.vertices[1].position={1,0,0};mesh.asset.vertices[2].position={0,1,0};mesh.asset.indices={0,1,2};mesh.asset.materials.emplace_back();mesh.asset.primitives.push_back({0,3,0,{}});
 tracing::Meshes meshes;meshes.emplace("triangle",mesh);
 scene::SceneDescription scene;scene.nodes.push_back({"parent","","",{10,0,0}});scene.nodes.push_back({"child","triangle","parent",{0,2,0}});
 auto output=tracing::buildGeometry(scene,meshes,{});require(output.triangles.size()==1&&output.triangles[0].p0[0]==10&&output.triangles[0].p0[1]==2,"Hierarchy transforms reach tracing geometry");
 require(output.triangles[0].n0[1]>.99F,"Interpolated vertex normals reach tracing geometry");
 auto fingerprint=tracing::sceneFingerprint(scene,{});scene.nodes[1].visible=false;require(tracing::sceneFingerprint(scene,{})!=fingerprint,"Visibility invalidates accumulation");require(tracing::buildGeometry(scene,meshes,{}).triangles.empty(),"Hidden geometry omitted");
 std::vector<unsigned> pixels;auto texture=tracing::appendTexture(pixels,{255,128,0,255},1,1,true);require(pixels[0]==0xff0080ff&&texture[3]==1,"Texture channel order and color space");
 bool rejected=false;try{tracing::appendTexture(pixels,{},1,1,false);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Malformed texture rejected");
 auto distribution=tracing::environmentDistribution(1,1,{0x3c00,0x3c00,0x3c00,0x3c00},8,4);
 require(distribution.size()==64,"Environment stores probability and cumulative arrays");
 require(std::abs(distribution[31]-1.F)<1e-6F,"Environment distribution integrates to one");
 float total=0;for(unsigned i=32;i<64;++i)total+=distribution[i];require(std::abs(total-1.F)<1e-5F,"Environment cell probabilities sum to one");
 std::cout<<"Path tracing BVH, hierarchy, visibility and atlas passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
