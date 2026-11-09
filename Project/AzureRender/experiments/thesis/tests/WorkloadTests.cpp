#include "Workload.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
static void require(bool value) { if (!value) throw std::runtime_error("workload contract failed"); }
int main() {
    for (auto spec : {std::pair<int,int>{32,16}, {128,64}}) {
        auto mesh = thesis::sphere(spec.first, spec.second);
        require(mesh.size() == size_t(spec.first == 32 ? 2880 : 48384));
        for (auto v : mesh) {
            double r = v.x*v.x + v.y*v.y + v.z*v.z;
            require(std::isfinite(r) && std::abs(r-1.0) < 1e-5);
        }
        for (size_t i=0; i<mesh.size(); i+=3) {
            auto a=mesh[i], b=mesh[i+1], c=mesh[i+2];
            double ux=b.x-a.x, uy=b.y-a.y, uz=b.z-a.z;
            double vx=c.x-a.x, vy=c.y-a.y, vz=c.z-a.z;
            double x=uy*vz-uz*vy, y=uz*vx-ux*vz, z=ux*vy-uy*vx;
            require(x*x+y*y+z*z > 1e-12);
        }
    }
    bool rejected=false;
    try { thesis::sphere(2,1); } catch (const std::invalid_argument&) { rejected=true; }
    require(rejected);
    std::cout << "Mesh sizes, finite unit vertices, nondegenerate triangles and invalid input passed\n";
}
