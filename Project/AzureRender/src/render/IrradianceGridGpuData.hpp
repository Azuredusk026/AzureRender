#pragma once
#include "render/IrradianceVolume.hpp"

namespace azurerender {
inline constexpr unsigned irradianceGridQuadraticFlag=8U;

inline unsigned irradianceGridQueryFlags(unsigned samplingFlags,IrradianceBasis basis) {
    if((samplingFlags&~7U)!=0U||(basis!=IrradianceBasis::Linear&&basis!=IrradianceBasis::Quadratic))
        throw std::invalid_argument("Invalid irradiance GPU query flags");
    if(!samplingFlags)
        return 0;
    return samplingFlags|(basis==IrradianceBasis::Quadratic?irradianceGridQuadraticFlag:0U);
}

inline std::vector<std::array<float,4>> packIrradianceGridCells(const IrradianceVolume& volume) {
    volume.validate();
    const bool quadratic=volume.basis==IrradianceBasis::Quadratic;
    const std::size_t stride=quadratic?8:4;
    std::vector<std::array<float,4>> rows(volume.cells.size()*stride);
    for(std::size_t index=0;index<volume.cells.size();++index) {
        const auto& cell=volume.cells[index];
        if(quadratic) {
            const auto& coefficients=volume.quadraticCells[index];
            for(unsigned color=0;color<3;++color) {
                rows[index*stride+2*color]=cell.coefficients[color];
                std::copy_n(coefficients[color].begin(),4,rows[index*stride+2*color+1].begin());
            }
            rows[index*stride+6]={coefficients[0][4],coefficients[1][4],coefficients[2][4],cell.weight};
        } else {
            for(unsigned color=0;color<3;++color)
                rows[index*stride+color]=cell.coefficients[color];
            rows[index*stride+3]={cell.weight,0,0,0};
        }
    }
    return rows;
}
}
