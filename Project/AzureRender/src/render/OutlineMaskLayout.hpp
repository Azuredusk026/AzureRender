#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace azurerender {
struct OutlineMaskLayout {
    std::uint32_t width=0, height=0, columns=0;
    std::vector<std::uint32_t> instanceTiles;
};

// Each masked instance owns a full camera footprint. Other instances cannot
// erase its outline when their silhouettes overlap in the camera view.
inline OutlineMaskLayout makeOutlineMaskLayout(std::size_t instances,
    const std::vector<std::uint32_t>& masked, std::uint32_t viewWidth,
    std::uint32_t viewHeight, std::uint32_t maxDimension,
    std::uint64_t byteBudget=128ULL*1024*1024) {
    OutlineMaskLayout result;
    result.instanceTiles.assign(instances,std::numeric_limits<std::uint32_t>::max());
    if(masked.empty())return result;
    if(!viewWidth||!viewHeight||viewWidth>maxDimension||viewHeight>maxDimension)
        throw std::invalid_argument("Outline mask viewport exceeds device dimensions");
    const auto maxColumns=maxDimension/viewWidth,maxRows=maxDimension/viewHeight;
    if(masked.size()>std::uint64_t{maxColumns}*maxRows)
        throw std::length_error("Outline mask instance tiles exceed device dimensions");
    result.columns=std::min(maxColumns,std::max(1U,static_cast<std::uint32_t>(
        std::ceil(std::sqrt(static_cast<double>(masked.size())*viewHeight/viewWidth)))));
    result.columns=std::max(result.columns,static_cast<std::uint32_t>((masked.size()+maxRows-1)/maxRows));
    const auto rows=static_cast<std::uint32_t>((masked.size()+result.columns-1)/result.columns);
    result.width=result.columns*viewWidth;result.height=rows*viewHeight;
    if(std::uint64_t{result.width}*result.height>byteBudget)
        throw std::length_error("Outline mask exceeds 128 MiB frame budget");
    for(std::size_t tile=0;tile<masked.size();++tile){
        const auto instance=masked[tile];
        if(instance>=instances)throw std::out_of_range("Outline mask source instance");
        if(result.instanceTiles[instance]!=std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Outline mask source instance repeated");
        result.instanceTiles[instance]=static_cast<std::uint32_t>(tile);
    }
    return result;
}
} // namespace azurerender
