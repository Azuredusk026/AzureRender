#include "render/pathtracing/TraceScene.hpp"
#include <iostream>
#include "assets/MaterialData.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <stdexcept>
#include "animation/ExpressionPose.hpp"
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 static_assert(sizeof(tracing::Triangle)==176);static_assert(sizeof(tracing::BvhNode)==48);static_assert(sizeof(tracing::Material)==128);
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
 scene.lights.push_back({"spot","",{0,1,0}});
 const auto lightFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.type=LightKind::Spot;
 require(tracing::sceneFingerprint(scene,{})!=lightFingerprint,"Light shape must invalidate tracing history");
 const auto shapeFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.falloff=LightFalloff::RangePower;
 require(tracing::sceneFingerprint(scene,{})!=shapeFingerprint,"Distance model invalidates tracing history");
 const auto responseFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.specularScale=.195F;
 require(tracing::sceneFingerprint(scene,{})!=responseFingerprint,"Light response invalidates tracing history");
 require(tracing::buildGeometry(scene,meshes,{}).lights.back().boxParameters[2]==.195F,"Tracing receives light response");
 const auto receiverFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.receivers.surfaces=false;
 require(tracing::sceneFingerprint(scene,{})!=receiverFingerprint,"Receiver selection invalidates tracing history");
 require(tracing::buildGeometry(scene,meshes,{}).lights.back().boxParameters[3]==2.F,"Tracing receives the public receiver mask");
 scene.lights[0].shape.receivers.surfaces=true;
 const auto falloffFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.falloffExponent=3.5F;
 require(tracing::sceneFingerprint(scene,{})!=falloffFingerprint,"Falloff exponent invalidates tracing history");
 const auto tracingLight=tracing::buildGeometry(scene,meshes,{}).lights.back();
 require(tracingLight.coneType[2]==2.F && tracingLight.coneType[3]==3.5F,"Tracing consumes the explicit distance model");
 scene.lights[0].shape.type=static_cast<LightKind>(2);
 scene.lights[0].shape.direction={0,1,0};
 const auto directionalLight=tracing::buildGeometry(scene,meshes,{}).lights.back();
 require(directionalLight.coneType[1]==2.F&&directionalLight.directionOuter[1]==1.F,"Tracing preserves directional emitter direction");
 const auto volumeFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.influenceBox.enabled=true;
 require(tracing::sceneFingerprint(scene,{})!=volumeFingerprint,"Influence activation invalidates tracing history");
 const auto boxFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.influenceBox.worldToBox[0][3]=3;
 require(tracing::sceneFingerprint(scene,{})!=boxFingerprint,"Influence transform invalidates tracing history");
 const auto thresholdFingerprint=tracing::sceneFingerprint(scene,{});
 scene.lights[0].shape.influenceBox.falloffThreshold=.5F;
 require(tracing::sceneFingerprint(scene,{})!=thresholdFingerprint,"Influence fade invalidates tracing history");
 const auto shapedLight=tracing::buildGeometry(scene,meshes,{}).lights.back();
 require(shapedLight.boxParameters[0]==1&&shapedLight.boxParameters[1]==.5F&&shapedLight.boxRows[0][3]==3,"Tracing retains the world-space influence volume");
 scene.nodes[1].scale={-1,1,1};auto mirrored=tracing::buildGeometry(scene,meshes,{});
 require(mirrored.triangles[0].e1[0]*mirrored.triangles[0].e2[1]-mirrored.triangles[0].e1[1]*mirrored.triangles[0].e2[0]>0,"Mirrored instances preserve front-face winding");scene.nodes[1].scale={1,1,1};
 auto fingerprint=tracing::sceneFingerprint(scene,{});scene.nodes[1].visible=false;require(tracing::sceneFingerprint(scene,{})!=fingerprint,"Visibility invalidates accumulation");require(tracing::buildGeometry(scene,meshes,{}).triangles.empty(),"Hidden geometry omitted");
 NodeAnimationFrame frame;frame.node="child";frame.morphOverrides={0,0,1};
 auto morphFingerprint=tracing::sceneFingerprint(scene,{frame});frame.morphOverrides[2]=.5F;
 require(tracing::sceneFingerprint(scene,{frame})!=morphFingerprint,"Morph weight changes invalidate tracing");
 auto expression=std::make_shared<ExpressionPose>();expression->id="face";expression->skeletonFingerprint="fixture";frame.expression=expression;
 auto expressionFingerprint=tracing::sceneFingerprint(scene,{frame});expression->bones.push_back({"jaw",{0,.1F,0},{0,0,0,1}});
 require(tracing::sceneFingerprint(scene,{frame})!=expressionFingerprint,"Expression contents invalidate tracing");
 frame.expression.reset();frame.morphOverrides={0,0,.5F};scene.nodes[1].visible=true;
 auto& morphAsset=meshes.at("triangle").asset;MorphTargetRange range;range.vertexCount=3;range.targets.resize(3);
 range.targets[2].positions={{{0,0,4}},{{0,0,4}},{{0,0,4}}};
 range.targets[2].normals={{{0,-2,2}},{{0,-2,2}},{{0,-2,2}}};
 morphAsset.morphRanges={range};morphAsset.defaultMorphWeights={{0,0,0}};
 auto morphed=tracing::buildGeometry(scene,meshes,{frame});
 require(morphed.triangles[0].p0[2]==2&&morphed.triangles[0].n0[2]>.99F,"Unskinned third target position and normal reach tracing");
 {
  auto animated=mesh;auto& asset=animated.asset;asset.hasSkin=true;asset.nodes.resize(1);asset.jointNodes={0};
  const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  asset.inverseBindMatrices={identity};asset.nodeWorldMatrices={identity};
  for(auto& vertex:asset.vertices){vertex.weights={1,0,0,0};vertex.joints={0,0,0,0};}
  AssetAnimation clip;clip.endTime=2;clip.samplers.push_back({{0,2},{{0,0,0,0},{4,0,0,0}}});
  clip.channels.push_back({0,0,AssetAnimationPath::Translation});asset.animations={clip};
  scene::SceneDescription weightedScene;weightedScene.nodes.push_back({"actor","animated",""});
  tracing::Meshes weightedMeshes;weightedMeshes.emplace("animated",std::move(animated));
  NodeAnimationFrame weightedFrame;weightedFrame.node="actor";
  weightedFrame.samples={{0,0,.25F,false,2},{0,2,.75F,false,2}};
  const auto weightedGeometry=tracing::buildGeometry(weightedScene,weightedMeshes,{weightedFrame});
  require(std::abs(weightedGeometry.triangles[0].p0[0]-3)<1e-6F,"Weighted skin pose reaches tracing geometry");
  for(int field=0;field<5;++field){auto changed=weightedFrame;auto& sample=changed.samples[0];
   if(field==0)sample.clip=1;if(field==1)sample.time=.1;if(field==2)sample.weight=.3F;
   if(field==3)sample.loop=true;if(field==4)sample.duration=1;
   require(tracing::sceneFingerprint(weightedScene,{changed})!=tracing::sceneFingerprint(weightedScene,{weightedFrame}),
       "Every weighted sample field invalidates tracing history");
  }
 }
 std::vector<unsigned> pixels;auto texture=tracing::appendTexture(pixels,{255,128,0,255},1,1,true);require(pixels[0]==0xff0080ff&&texture[3]==1,"Texture channel order and color space");
 bool rejected=false;try{tracing::appendTexture(pixels,{},1,1,false);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Malformed texture rejected");
 auto distribution=tracing::environmentDistribution(1,1,{0x3c00,0x3c00,0x3c00,0x3c00},8,4);
 require(distribution.size()==64,"Environment stores probability and cumulative arrays");
 require(std::abs(distribution[31]-1.F)<1e-6F,"Environment distribution integrates to one");
 float total=0;for(unsigned i=32;i<64;++i)total+=distribution[i];require(std::abs(total-1.F)<1e-5F,"Environment cell probabilities sum to one");
 auto glass=MaterialData::defaults("azure.pbr");glass.parameters["transmission"]=1.;glass.parameters["ior"]=1.33;glass.parameters["thickness"]=.1;
 const auto interpreted=glass.interpret([](const auto&){return std::filesystem::path();});
 require(interpreted.transmission==1&&std::abs(interpreted.ior-1.33F)<1e-5F,"Material service retains physical transmission");
 RenderSettings settings;settings.pathTraceDenoise=true;settings.pathTraceSeed=17;RenderSettings reopened;
 settings.sceneLightingEnabled=true;
 decodeLevelRenderSettings(reopened,encodeLevelRenderSettings(settings));require(reopened.pathTraceDenoise&&reopened.pathTraceSeed==17,"Denoise and seed survive settings persistence");
 require(reopened.sceneLightingEnabled,"Explicit scene lighting survives settings persistence");
 std::cout<<"Path tracing BVH, hierarchy, visibility and atlas passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
