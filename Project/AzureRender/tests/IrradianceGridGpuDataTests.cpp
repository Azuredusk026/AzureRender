#include "render/IrradianceGridGpuData.hpp"
#include "render/IrradianceResource.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace azurerender;
int main(int argc,char** argv) {
    assert(argc==2);
    for(auto basis:{IrradianceBasis::Linear,IrradianceBasis::Quadratic}) {
        IrradianceVolume volume;
        volume.basis=basis;
        volume.cells.resize(1);
        volume.cells[0].weight=.75F;
        volume.cells[0].coefficients={{{1,2,3,4},{5,6,7,8},{9,10,11,12}}};
        if(basis==IrradianceBasis::Quadratic)
            volume.quadraticCells.push_back({{{1,2,3,4,5},{6,7,8,9,10},{11,12,13,14,15}}});
        const auto rows=packIrradianceGridCells(volume);
        const auto encoded=volume.encode();
        const auto path=std::filesystem::u8path(argv[1]);
        {
            std::ofstream output(path,std::ios::binary|std::ios::trunc);
            output.write(reinterpret_cast<const char*>(encoded.data()),std::streamsize(encoded.size()));
            output.close();
            assert(output);
        }
        assert(readIrradianceResourceKind(path)==IrradianceResourceKind::Grid);
        assert(IrradianceVolume::load(path).encode()==encoded);
        assert(rows.size()==(basis==IrradianceBasis::Quadratic?8U:4U));
        assert(std::memcmp(rows.data(),encoded.data()+IrradianceVolume::headerBytes,rows.size()*sizeof(rows[0]))==0);
        assert(irradianceGridQueryFlags(0,basis)==0);
        assert(irradianceGridQueryFlags(7,basis)==(basis==IrradianceBasis::Quadratic?15U:7U));
        bool rejected=false;
        try {irradianceGridQueryFlags(8,basis);}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }
    std::cout<<"Irradiance CPU and GPU row layouts match both file versions\n";
}
