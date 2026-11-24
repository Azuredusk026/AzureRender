#pragma once
#include "ProceduralContracts.hpp"
#ifdef _MSC_VER
#pragma warning(push)
// The dependency's hash routine intentionally narrows a 64-bit hash word.
#pragma warning(disable:4244)
#endif
#include <manifold/manifold.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <chrono>
#include <set>
#include <random>
namespace azurerender::procedural {
using Json=nlohmann::json;
using namespace manifold;
void fields(const Json&,std::initializer_list<const char*>);
double finiteNumber(const Json&);
vec3 vector(const Json&);
vec3 position(vec3,bool scad,double scale=1);
vec3 scaling(vec3,bool scad);
quat rotation(vec3 degrees,bool scad);
Json values(vec3);
Json values(quat);
struct Evaluation {
    const ProceduralGeometryRequest& request;
    Json document,parameters,modules=Json::object();
    std::mt19937_64 random;
    std::size_t nodes=0,triangles=0;
    std::chrono::steady_clock::time_point deadline;
    std::set<std::string> moduleStack;
    explicit Evaluation(const ProceduralGeometryRequest&);
    void check();
    double number(const Json&,const Json& scope);
    vec3 triple(const Json&,const Json& scope);
    Manifold solid(const Json&,const Json& scope,std::size_t depth=1);
};
class GltfWriter {
public:
    Json data={{"asset",{{"version","2.0"},{"generator","AzureProcedural/1 Manifold/3.5.2"}}},
        {"scene",0},{"scenes",Json::array()},{"nodes",Json::array()},{"meshes",Json::array()},
        {"materials",Json::array()},{"accessors",Json::array()},{"bufferViews",Json::array()}};
    std::vector<unsigned char> bytes;
    std::size_t accessor(const std::vector<float>&,std::size_t stride,const std::string& type,bool bounds=false);
    std::size_t indices(const std::vector<std::uint32_t>&);
    std::size_t joints(const std::vector<std::uint16_t>&);
    Json primitive(const std::vector<vec3>&,const std::vector<std::uint32_t>&,std::size_t material,const std::vector<std::uint16_t>& joint={});
    std::size_t material(const Json&);
    std::string finish();
};
void mesh(const Manifold&,std::vector<vec3>&,std::vector<std::uint32_t>&);
}
