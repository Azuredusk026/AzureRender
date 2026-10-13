#include "assets/GeneratorRegistry.hpp"
#include "assets/GltfLoader.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){
    auto root=std::filesystem::temp_directory_path()/("azure-primitives-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);int status=0;
    try{
        const auto registry=GeneratorRegistry::builtins();
        for(const auto* kind:{"cube","plane","sphere","cylinder","capsule","torus"}){
            GenerationRequest request;request.parameters={{"kind",kind},{"size",{2,4,6}},{"segments",12}};request.outputReference="assets:/shape.glb";request.licenseSource="Azure primitive generator";
            const auto output=registry.generate("azure.primitive",request);const auto path=root/(std::string(kind)+".glb");
            std::ofstream(path,std::ios::binary).write(output.bytes.data(),static_cast<std::streamsize>(output.bytes.size()));
            const auto asset=loadGltfAsset(path.string());
            if(asset.vertices.empty()||asset.indices.empty())throw std::runtime_error("Primitive must produce actual loaded geometry");
            std::array<float,3> lo{100,100,100},hi{-100,-100,-100};
            for(const auto& vertex:asset.vertices){float norm=0;for(int axis=0;axis<3;++axis){lo[axis]=std::min(lo[axis],vertex.position[axis]);hi[axis]=std::max(hi[axis],vertex.position[axis]);norm+=vertex.normal[axis]*vertex.normal[axis];}
                if(std::abs(norm-1)>1e-4F)throw std::runtime_error("Primitive normals must remain unit vectors after nonuniform size");}
            const std::array<float,3> expected{2,kind==std::string("plane")?0.F:4.F,6};
            for(int axis=0;axis<3;++axis)if(std::abs(hi[axis]-lo[axis]-expected[axis])>1e-3F)throw std::runtime_error("Primitive dimensions do not match requested size");
            for(std::size_t i=0;i<asset.indices.size();i+=3){
                const auto& a=asset.vertices.at(asset.indices.at(i));const auto& b=asset.vertices.at(asset.indices.at(i+1));const auto& c=asset.vertices.at(asset.indices.at(i+2));std::array<float,3> u{},v{},normal{};
                for(int axis=0;axis<3;++axis){u[axis]=b.position[axis]-a.position[axis];v[axis]=c.position[axis]-a.position[axis];normal[axis]=a.normal[axis]+b.normal[axis]+c.normal[axis];}
                const std::array<float,3> cross{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};float area=0,dot=0;for(int axis=0;axis<3;++axis){area+=cross[axis]*cross[axis];dot+=cross[axis]*normal[axis];}
                if(area>1e-12F&&dot<=0)throw std::runtime_error("Primitive winding must agree with outward normals");
            }
            if(output.manifest.generatorId!="azure.primitive"||output.manifest.generatorVersion!=1)throw std::runtime_error("Primitive provenance must retain generator identity and version");
            if(registry.generate("azure.primitive",request).bytes!=output.bytes)throw std::runtime_error("Primitive generation must be deterministic");
            request.parameters["size"]={2,0,6};bool rejected=false;try{registry.generate("azure.primitive",request);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Zero dimensions must be rejected");
            request.parameters["size"]={2,4,6};request.parameters["segments"]=1000000;rejected=false;try{registry.generate("azure.primitive",request);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Unbounded tessellation must be rejected");
        }
        std::cout<<"Six loaded primitives, sizes, unit normals, deterministic provenance and input budgets passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
