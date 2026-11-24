#include "RigTextContracts.hpp"
#include "ProceduralInternal.hpp"
#include <algorithm>
#include <stdexcept>
namespace azurerender {
RigTextResult generateRigText(const RigTextRequest& request){
    using namespace procedural;Evaluation evaluation(request.geometry);
    if(!std::isfinite(request.scale)||request.scale<=0||request.scale>10000)throw std::invalid_argument("Rigid asset scale must be within (0,10000]");
    const auto& source=evaluation.document;
    if(source.contains("root"))throw std::invalid_argument("Rig source requires bones with attached geometry");
    const auto& bones=source.at("bones"),animations=source.at("animations");
    if(!bones.is_array()||bones.empty()||bones.size()>256||!animations.is_array()||animations.empty()||animations.size()>64)throw std::invalid_argument("Rig bone or animation count exceeds budget");
    const bool scad=source.value("coordinates",std::string("engine"))=="scad";
    struct Bone {std::string name;int parent=-1;vec3 translation,scale;quat rotation;mat4 world;};
    std::vector<Bone> rig;std::map<std::string,std::size_t> ids;
    for(const auto& bone:bones){
        fields(bone,{"name","parent","translation","rotation","scale","geometry","material"});
        const auto name=bone.at("name").get<std::string>();
        if(name.rfind("bone_",0)!=0||name.size()>128||!ids.emplace(name,rig.size()).second)throw std::invalid_argument("Rigid bones require unique bone_ names");
        Bone value;value.name=name;
        value.translation=position(evaluation.triple(bone.value("translation",Json::array({0,0,0})),evaluation.parameters),scad,request.scale);
        value.scale=scaling(evaluation.triple(bone.value("scale",Json::array({1,1,1})),evaluation.parameters),scad);
        if(value.scale.x<=0||value.scale.y<=0||value.scale.z<=0)throw std::invalid_argument("Rigid bind scale must be positive");
        value.rotation=rotation(evaluation.triple(bone.value("rotation",Json::array({0,0,0})),evaluation.parameters),scad);rig.push_back(value);
    }
    GltfWriter writer;Json roots=Json::array();
    for(std::size_t i=0;i<rig.size();++i){
        if(bones[i].contains("parent")){
            const auto name=bones[i].at("parent").get<std::string>();if(!ids.count(name))throw std::invalid_argument("Unknown rigid parent");rig[i].parent=static_cast<int>(ids.at(name));
        }else roots.push_back(i);
        writer.data["nodes"].push_back({{"name",rig[i].name},{"translation",values(rig[i].translation)},{"rotation",values(rig[i].rotation)},{"scale",values(rig[i].scale)}});
    }
    if(roots.size()!=1)throw std::invalid_argument("Rigid hierarchy requires one root");
    std::vector<int> state(rig.size(),0);
    const auto world=[&](std::size_t i,std::size_t depth,auto&& self)->mat4{
        evaluation.check();if(depth>request.geometry.budget.depth||state[i]==1)throw std::invalid_argument("Rigid hierarchy cycle or depth exceeded");
        if(state[i]==2)return rig[i].world;state[i]=1;
        auto transform=la::mul(la::translation_matrix(rig[i].translation),la::rotation_matrix(rig[i].rotation),la::scaling_matrix(rig[i].scale));
        if(rig[i].parent>=0)transform=la::mul(self(static_cast<std::size_t>(rig[i].parent),depth+1,self),transform);
        rig[i].world=transform;state[i]=2;return transform;
    };
    std::vector<float> inverseBind;Json jointIds=Json::array();
    for(std::size_t i=0;i<rig.size();++i){
        const auto matrix=world(i,1,world),inverse=la::inverse(matrix);
        for(int column=0;column<4;++column)for(int row=0;row<4;++row)inverseBind.push_back(static_cast<float>(inverse[column][row]));
        jointIds.push_back(i);
        if(rig[i].parent>=0){auto& parent=writer.data["nodes"][static_cast<std::size_t>(rig[i].parent)];if(!parent.contains("children"))parent["children"]=Json::array();parent["children"].push_back(i);}
    }
    Json primitives=Json::array();std::size_t triangles=0;
    for(std::size_t i=0;i<rig.size();++i){
        if(!bones[i].contains("geometry"))continue;
        const auto solid=evaluation.solid(bones[i].at("geometry"),evaluation.parameters);
        triangles+=solid.NumTri();if(triangles>request.geometry.budget.triangles)throw std::invalid_argument("Rigid asset total triangle budget exceeded");
        std::vector<vec3> points;std::vector<std::uint32_t> faces;mesh(solid,points,faces);
        for(auto& point:points)point=la::mul(rig[i].world,vec4(position(point,scad,request.scale),1)).xyz();
        const std::vector<std::uint16_t> ownership(points.size(),static_cast<std::uint16_t>(i));
        const auto material=writer.material(bones[i].value("material",source.value("material",Json::object())));
        primitives.push_back(writer.primitive(points,faces,material,ownership));
    }
    if(primitives.empty())throw std::invalid_argument("Rigid asset requires owned triangle geometry");
    writer.data["meshes"].push_back({{"primitives",primitives}});
    const auto meshNode=writer.data["nodes"].size();writer.data["nodes"].push_back({{"name","rig_geometry"},{"mesh",0},{"skin",0}});
    writer.data["skins"]=Json::array({{{"joints",jointIds},{"skeleton",roots[0]},{"inverseBindMatrices",writer.accessor(inverseBind,16,"MAT4")}}});
    roots.push_back(meshNode);writer.data["scenes"].push_back({{"nodes",roots}});
    writer.data["animations"]=Json::array();
    Json graph={{"schemaVersion",1},{"states",Json::array()},{"transitions",Json::array()}};
    std::set<std::string> names;std::size_t totalKeys=0;
    for(const auto& clip:animations){
        fields(clip,{"name","loop","channels"});const auto name=clip.at("name").get<std::string>();
        if(name.rfind("anim_",0)!=0||name.size()>128||!names.insert(name).second)throw std::invalid_argument("Text clips require unique anim_ names");
        const bool loop=clip.value("loop",true);const auto& channels=clip.at("channels");
        if(!channels.is_array()||channels.empty()||channels.size()>rig.size()*3)throw std::invalid_argument("Rig animation requires bounded channels");
        Json animation={{"name",name},{"extras",{{"azureLoop",loop}}},{"samplers",Json::array()},{"channels",Json::array()}};
        std::set<std::pair<std::string,std::string>> targets;
        for(const auto& channel:channels){
            evaluation.check();fields(channel,{"bone","path","keys"});const auto bone=channel.at("bone").get<std::string>(),path=channel.at("path").get<std::string>();
            if(!ids.count(bone)||!targets.emplace(bone,path).second||(path!="pos"&&path!="rot"&&path!="scale"))throw std::invalid_argument("Unknown rig channel or duplicate target");
            const auto i=ids.at(bone);const auto& keys=channel.at("keys");
            if(!keys.is_array()||keys.empty()||keys.size()>4096||(totalKeys+=keys.size())>65536)throw std::invalid_argument("Rig keyframe budget exceeded");
            std::vector<float> times,outputs;double previous=-1;
            for(const auto& key:keys){
                if(!key.is_array()||key.size()!=2)throw std::invalid_argument("Rig key requires time and value");
                const auto time=finiteNumber(key[0]);
                if(time<0||time>3600||time<=previous)throw std::invalid_argument("Rig key times must increase within 0..3600 seconds");
                const auto encoded=static_cast<float>(time);if(!times.empty()&&encoded<=times.back())throw std::invalid_argument("Rig key times collapse at asset precision");
                previous=time;times.push_back(encoded);const auto value=evaluation.triple(key[1],evaluation.parameters);
                if(path=="rot"){
                    const auto q=la::qmul(rig[i].rotation,rotation(value,scad));for(int axis=0;axis<4;++axis)outputs.push_back(static_cast<float>(q[axis]));
                }else{
                    vec3 v;
                    if(path=="pos")v=rig[i].translation+la::qrot(rig[i].rotation,rig[i].scale*position(value,scad,request.scale));
                    else{if(value.x<=0||value.y<=0||value.z<=0)throw std::invalid_argument("Rig animated scale must be positive");v=rig[i].scale*scaling(value,scad);}
                    for(int axis=0;axis<3;++axis)outputs.push_back(static_cast<float>(v[axis]));
                }
            }
            const auto sampler=animation["samplers"].size();
            animation["samplers"].push_back({{"input",writer.accessor(times,1,"SCALAR",true)},{"output",writer.accessor(outputs,path=="rot"?4:3,path=="rot"?"VEC4":"VEC3")},{"interpolation","LINEAR"}});
            animation["channels"].push_back({{"sampler",sampler},{"target",{{"node",i},{"path",path=="rot"?"rotation":path=="pos"?"translation":"scale"}}}});
        }
        graph["states"].push_back({{"name",name},{"clip",writer.data["animations"].size()},{"loop",loop}});writer.data["animations"].push_back(animation);
    }
    graph["initial"]=graph["states"][0]["name"];
    writer.data["extras"]={{"azureAnimationGraph",graph},{"azureProcedural",{{"schemaVersion",1},{"compiler","Manifold/3.5.2"},{"coordinates",scad?"scad":"engine"},{"scale",request.scale},{"seed",request.geometry.seed}}}};
    evaluation.check();return {writer.finish(),graph};
}
}
