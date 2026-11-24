#pragma once
#include <cmath>
#include <stdexcept>
#include <vector>

namespace thesis {
struct Vertex { float x, y, z; };
inline std::vector<Vertex> sphere(int longitude, int latitude) {
    if (longitude < 3 || latitude < 2 || longitude > 1024 || latitude > 1024)
        throw std::invalid_argument("sphere tessellation out of range");
    constexpr double pi = 3.14159265358979323846;
    auto point = [&](int x, int y) {
        double a=2*pi*x/longitude, b=pi*y/latitude;
        return Vertex{float(std::sin(b)*std::cos(a)), float(std::cos(b)), float(std::sin(b)*std::sin(a))};
    };
    std::vector<Vertex> vertices;
    vertices.reserve(size_t(longitude)*size_t(latitude-1)*6);
    for (int y=0; y<latitude; ++y) for (int x=0; x<longitude; ++x) {
        auto a=point(x,y), b=point(x+1,y), c=point(x,y+1), d=point(x+1,y+1);
        if (y>0) vertices.insert(vertices.end(), {a,b,c});
        if (y<latitude-1) vertices.insert(vertices.end(), {b,d,c});
    }
    return vertices;
}
}
