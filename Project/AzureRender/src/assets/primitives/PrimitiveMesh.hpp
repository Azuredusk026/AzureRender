#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <functional>
#include <string>
namespace azurerender::primitives {
using Vec=std::array<float,3>;
struct Mesh {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    Vec minimum{1e20F,1e20F,1e20F},maximum{-1e20F,-1e20F,-1e20F};
    std::uint32_t vertex(Vec position,Vec normal,float u,float v){
        const auto id=static_cast<std::uint32_t>(vertices.size()/8);float length=0;
        for(float x:normal)length+=x*x;length=std::sqrt(length);
        for(int axis=0;axis<3;++axis){minimum[axis]=std::min(minimum[axis],position[axis]);maximum[axis]=std::max(maximum[axis],position[axis]);vertices.push_back(position[axis]);}
        for(float x:normal)vertices.push_back(x/length);vertices.push_back(u);vertices.push_back(v);return id;
    }
    void triangle(std::uint32_t a,std::uint32_t b,std::uint32_t c){indices.insert(indices.end(),{a,b,c});}
};
bool roundPrimitive(Mesh& mesh,const std::string& kind,const Vec& size,unsigned segments,const std::function<void()>& check);
}
