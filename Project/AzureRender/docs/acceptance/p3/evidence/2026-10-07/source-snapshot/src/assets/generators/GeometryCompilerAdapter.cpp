#include "ProceduralInternal.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender::procedural {
void fields(const Json& value,std::initializer_list<const char*> allowed){
    if(!value.is_object())throw std::invalid_argument("Procedural object required");
    for(const auto& item:value.items())if(std::none_of(allowed.begin(),allowed.end(),[&](const char* name){return item.key()==name;}))
        throw std::invalid_argument("Unknown procedural field: "+item.key());
}
double finiteNumber(const Json& value){
    if(!value.is_number())throw std::invalid_argument("Procedural number required");
    const auto n=value.get<double>();
    if(!std::isfinite(n)||std::abs(n)>1000000)throw std::invalid_argument("Procedural number outside finite coordinate budget");
    return n;
}
vec3 vector(const Json& value){
    if(!value.is_array()||value.size()!=3)throw std::invalid_argument("Procedural vector requires three numbers");
    return {finiteNumber(value[0]),finiteNumber(value[1]),finiteNumber(value[2])};
}
vec3 position(vec3 v,bool scad,double scale){return (scad?vec3(v.x,v.z,-v.y):v)*scale;}
vec3 scaling(vec3 v,bool scad){return scad?vec3(v.x,v.z,v.y):v;}
quat rotation(vec3 degrees,bool scad){
    constexpr double radians=3.14159265358979323846/180.;
    auto result=la::qmul(la::rotation_quat(vec3(0,0,1),degrees.z*radians),
        la::rotation_quat(vec3(0,1,0),degrees.y*radians),la::rotation_quat(vec3(1,0,0),degrees.x*radians));
    if(scad){const auto basis=la::rotation_quat(vec3(1,0,0),-3.14159265358979323846/2);result=la::qmul(basis,result,la::qconj(basis));}
    return result;
}
Json values(vec3 v){return Json::array({v.x,v.y,v.z});}
Json values(quat v){return Json::array({v.x,v.y,v.z,v.w});}
namespace {
Json parameters(const Json& declarations,const Json& overrides,const Json& inherited=Json::object()){
    if(!declarations.is_object()||!overrides.is_object())throw std::invalid_argument("Procedural parameters require objects");
    auto result=inherited;
    for(const auto& item:overrides.items())if(!declarations.contains(item.key()))throw std::invalid_argument("Unknown procedural parameter: "+item.key());
    for(const auto& item:declarations.items()){
        fields(item.value(),{"default","minimum","maximum"});
        const double low=finiteNumber(item.value().at("minimum")),high=finiteNumber(item.value().at("maximum"));
        const auto fallback=finiteNumber(item.value().at("default"));
        const auto n=finiteNumber(overrides.value(item.key(),Json(fallback)));
        if(low>high||fallback<low||fallback>high||n<low||n>high)throw std::invalid_argument("Procedural parameter exceeds declared range: "+item.key());
        result[item.key()]=n;
    }
    return result;
}
void reference(const std::string& ref){
    if(ref.empty()||ref.size()>1024||ref.find("..")!=std::string::npos||ref.find('\\')!=std::string::npos||ref.front()=='/'||ref.find('\0')!=std::string::npos||ref.find(":/")==std::string::npos)
        throw std::invalid_argument("Dependency requires a bounded virtual reference");
}
}
Evaluation::Evaluation(const ProceduralGeometryRequest& r):request(r),random(r.seed){
    const auto& b=r.budget;
    if(b.schemaVersion!=1||!b.sourceBytes||b.sourceBytes>2*1024*1024||!b.depth||b.depth>64||!b.triangles||b.triangles>1000000||!b.timeoutMs||b.timeoutMs>60000)
        throw std::invalid_argument("Invalid procedural budget version or limits");
    deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(b.timeoutMs);check();
    std::size_t size=r.source.size();if(size>b.sourceBytes)throw std::invalid_argument("Procedural source budget exceeded");
    document=Json::parse(r.source);
    fields(document,{"schemaVersion","coordinates","parameters","modules","imports","root","material","bones","animations"});
    if(!document.at("schemaVersion").is_number_integer()||document.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported procedural source version");
    const auto coordinates=document.value("coordinates",std::string("engine"));
    if(coordinates!="engine"&&coordinates!="scad")throw std::invalid_argument("Unsupported procedural coordinate space");
    std::map<std::string,Json> dependencies;
    if(r.dependencies.size()>1023)throw std::invalid_argument("Procedural dependency count exceeded");
    for(const auto& input:r.dependencies){
        reference(input.reference);size+=input.bytes.size();if(size>b.sourceBytes)throw std::invalid_argument("Procedural dependency source budget exceeded");
        if(!dependencies.emplace(input.reference,Json::parse(input.bytes)).second)throw std::invalid_argument("Duplicate procedural dependency");
    }
    std::set<std::string> visiting,visited;
    const auto merge=[&](const Json& d,std::size_t depth,auto&& self)->void{
        check();if(depth>b.depth)throw std::invalid_argument("Procedural dependency depth exceeded");
        if(!d.is_object()||!d.at("schemaVersion").is_number_integer()||d.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported dependency version");
        const auto imports=d.value("imports",Json::array());if(!imports.is_array())throw std::invalid_argument("Imports require an array");
        for(const auto& value:imports){
            const auto ref=value.get<std::string>();reference(ref);
            if(!dependencies.count(ref))throw std::invalid_argument("Unresolved procedural dependency: "+ref);
            if(visiting.count(ref))throw std::invalid_argument("Procedural dependency cycle");
            if(!visited.count(ref)){visiting.insert(ref);fields(dependencies.at(ref),{"schemaVersion","imports","modules"});self(dependencies.at(ref),depth+1,self);visiting.erase(ref);visited.insert(ref);}
        }
        const auto definitions=d.value("modules",Json::object());if(!definitions.is_object())throw std::invalid_argument("Modules require an object");
        for(const auto& item:definitions.items()){
            if(item.key().empty()||item.key().size()>128||modules.contains(item.key())||modules.size()>=1024)throw std::invalid_argument("Duplicate module or module budget exceeded");
            modules[item.key()]=item.value();
        }
    };
    merge(document,1,merge);parameters=procedural::parameters(document.value("parameters",Json::object()),r.parameters);
}
void Evaluation::check(){
    if(request.check)request.check();
    if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Procedural compilation deadline exceeded");
}
double Evaluation::number(const Json& v,const Json& scope){
    if(v.is_number())return finiteNumber(v);
    fields(v,{"parameter","random"});
    if(v.size()!=1)throw std::invalid_argument("Numeric expressions require one operator");
    if(v.contains("parameter")){const auto name=v.at("parameter").get<std::string>();if(!scope.contains(name))throw std::invalid_argument("Unknown parameter reference");return finiteNumber(scope.at(name));}
    const auto& bounds=v.at("random");if(!bounds.is_array()||bounds.size()!=2)throw std::invalid_argument("Random expression requires bounds");
    const auto lo=finiteNumber(bounds[0]),hi=finiteNumber(bounds[1]);if(lo>hi)throw std::invalid_argument("Invalid random bounds");
    const auto unit=static_cast<double>(random()>>11)*(1./9007199254740992.);return lo+(hi-lo)*unit;
}
vec3 Evaluation::triple(const Json& v,const Json& scope){
    if(!v.is_array()||v.size()!=3)throw std::invalid_argument("Procedural expression vector requires three components");
    return {number(v[0],scope),number(v[1],scope),number(v[2],scope)};
}
Manifold Evaluation::solid(const Json& node,const Json& scope,std::size_t depth){
    check();if(depth>request.budget.depth||++nodes>16384)throw std::invalid_argument("Procedural expansion budget exceeded");
    fields(node,{"op","size","center","radius","height","segments","children","name","arguments","translate","rotate","scale","vertices","triangles"});
    const auto op=node.at("op").get<std::string>();Manifold result;
    for(const auto& field:node.items()){
        const auto& key=field.key();
        if(key=="op"||key=="translate"||key=="rotate"||key=="scale")continue;
        const bool allowed=op=="cube"?(key=="size"||key=="center"):
            op=="sphere"?(key=="radius"||key=="segments"):
            op=="cylinder"?(key=="radius"||key=="height"||key=="segments"||key=="center"):
            (op=="union"||op=="difference"||op=="intersection")?key=="children":
            op=="module"?(key=="name"||key=="arguments"):
            op=="mesh"?(key=="vertices"||key=="triangles"):false;
        if(!allowed)throw std::invalid_argument("Field does not belong to procedural operator "+op+": "+key);
    }
    if(op=="cube"){
        const auto size=triple(node.at("size"),scope);if(size.x<=0||size.y<=0||size.z<=0)throw std::invalid_argument("Cube dimensions must be positive");
        result=Manifold::Cube(size,node.value("center",false));
    }else if(op=="sphere"||op=="cylinder"){
        const auto radius=number(node.at("radius"),scope),height=number(node.value("height",Json(1)),scope);
        const auto segments=node.value("segments",32);
        if(radius<=0||height<=0||segments<4||segments>256||!node.value("segments",Json(32)).is_number_integer())throw std::invalid_argument("Invalid curved primitive dimensions or tessellation");
        result=op=="sphere"?Manifold::Sphere(radius,segments):Manifold::Cylinder(height,radius,radius,segments,node.value("center",false));
    }else if(op=="union"||op=="difference"||op=="intersection"){
        const auto& children=node.at("children");if(!children.is_array()||children.empty()||children.size()>1024)throw std::invalid_argument("Boolean requires bounded children");
        for(std::size_t i=0;i<children.size();++i){
            auto child=solid(children[i],scope,depth+1);
            if(i==0)result=std::move(child);else result=op=="union"?result+child:op=="difference"?result-child:result^child;
            check();if(result.Status()!=Manifold::Error::NoError||result.NumTri()>request.budget.triangles)throw std::invalid_argument("Boolean intermediate exceeds manifold or triangle contract");
        }
    }else if(op=="module"){
        const auto name=node.at("name").get<std::string>();
        if(!modules.contains(name)||moduleStack.count(name))throw std::invalid_argument("Unknown module or recursive module cycle: "+name);
        const auto& definition=modules.at(name);auto local=scope;
        const auto arguments=node.value("arguments",Json::object());
        if(!arguments.is_object())throw std::invalid_argument("Module arguments require an object");
        Json resolved=Json::object();for(const auto& item:arguments.items())resolved[item.key()]=number(item.value(),scope);
        moduleStack.insert(name);
        if(definition.contains("body")){fields(definition,{"parameters","body"});local=procedural::parameters(definition.value("parameters",Json::object()),resolved,scope);result=solid(definition.at("body"),local,depth+1);}
        else{if(!resolved.empty())throw std::invalid_argument("Module has no argument declarations");result=solid(definition,scope,depth+1);}
        moduleStack.erase(name);
    }else if(op=="mesh"){
        const auto& vertices=node.at("vertices");const auto& indices=node.at("triangles");
        if(!vertices.is_array()||!indices.is_array()||vertices.size()>request.budget.triangles*3||indices.size()>request.budget.triangles)throw std::invalid_argument("Mesh input exceeds complexity budget");
        MeshGL input;
        for(const auto& v:vertices){const auto point=triple(v,scope);for(int axis=0;axis<3;++axis)input.vertProperties.push_back(static_cast<float>(point[axis]));}
        for(const auto& t:indices){if(!t.is_array()||t.size()!=3)throw std::invalid_argument("Mesh faces must be triangles");for(const auto& i:t){if(!i.is_number_integer()||i.get<std::int64_t>()<0||i.get<std::uint64_t>()>=vertices.size())throw std::invalid_argument("Mesh vertex index outside input");input.triVerts.push_back(i.get<std::uint32_t>());}}
        result=Manifold(input);
    }else throw std::invalid_argument("Unsupported procedural operation: "+op);
    if(node.contains("scale")){const auto s=triple(node.at("scale"),scope);if(s.x==0||s.y==0||s.z==0)throw std::invalid_argument("Solid scale must be invertible");result=result.Scale(s);}
    if(node.contains("rotate")){const auto r=triple(node.at("rotate"),scope);result=result.Rotate(r.x,r.y,r.z);}
    if(node.contains("translate"))result=result.Translate(triple(node.at("translate"),scope));
    check();if(result.Status()!=Manifold::Error::NoError)throw std::invalid_argument("Geometry is not a finite closed manifold");
    const auto count=result.NumTri();if(count>request.budget.triangles)throw std::invalid_argument("Intermediate solid triangle budget exceeded");
    if(op=="cube"||op=="sphere"||op=="cylinder"||op=="mesh"){
        triangles+=count;if(triangles>request.budget.triangles)throw std::invalid_argument("Expanded primitive triangle budget exceeded");
    }
    if(result.IsEmpty()||result.Volume()<=1e-12)throw std::invalid_argument("Geometry is empty or degenerate");
    check();return result;
}
void mesh(const Manifold& solid,std::vector<vec3>& positions,std::vector<std::uint32_t>& indices){
    const auto data=solid.GetMeshGL();
    for(std::size_t i=0;i<data.vertProperties.size();i+=data.numProp)positions.emplace_back(data.vertProperties[i],data.vertProperties[i+1],data.vertProperties[i+2]);
    indices=data.triVerts;
}
}
namespace azurerender {
ProceduralGeometryResult compileProceduralGeometry(const ProceduralGeometryRequest& request){
    using namespace procedural;Evaluation evaluation(request);
    if(evaluation.document.contains("bones")||evaluation.document.contains("animations"))throw std::invalid_argument("Geometry source requires the geometry domain");
    const auto solid=evaluation.solid(evaluation.document.at("root"),evaluation.parameters);
    std::vector<vec3> positions;std::vector<std::uint32_t> indices;mesh(solid,positions,indices);
    const bool scad=evaluation.document.value("coordinates",std::string("engine"))=="scad";
    for(auto& point:positions)point=position(point,scad);
    GltfWriter writer;const auto material=writer.material(evaluation.document.value("material",Json::object()));
    writer.data["meshes"].push_back({{"primitives",Json::array({writer.primitive(positions,indices,material)})}});
    writer.data["nodes"].push_back({{"name","procedural_geometry"},{"mesh",0}});writer.data["scenes"].push_back({{"nodes",{0}}});
    writer.data["extras"]["azureProcedural"]={{"schemaVersion",1},{"compiler","Manifold/3.5.2"},{"seed",request.seed},{"coordinates",scad?"scad":"engine"},{"volume",solid.Volume()}};
    evaluation.check();return {writer.finish(),indices.size()/3,solid.Volume()};
}
}
