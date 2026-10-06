#include "VisibilityPrototype.hpp"
#include <cmath>
#include <utility>
namespace azurerender {
VisibilityPrototype::VisibilityPrototype(VisibilityFrameKey key,
    std::vector<GpuCullBounds> bounds,std::vector<VkDrawIndexedIndirectCommand> commands,
    std::vector<GpuSurfaceIdentity> surfaces)
    :key_(key),bounds_(std::move(bounds)),commands_(std::move(commands)),surfaces_(std::move(surfaces)) {
    validateSources(bounds_,commands_,surfaces_);
}
void VisibilityPrototype::validateSources(const std::vector<GpuCullBounds>& bounds,
    const std::vector<VkDrawIndexedIndirectCommand>& commands,const std::vector<GpuSurfaceIdentity>& surfaces) {
    if(commands.size()!=surfaces.size())throw std::invalid_argument("Surface and command counts differ");
    for(const auto& box:bounds) for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(box.minimum[axis]) || !std::isfinite(box.maximum[axis]) || box.minimum[axis]>box.maximum[axis])
            throw std::invalid_argument("Visibility bounds must be finite and ordered");
    for(std::size_t slot=0;slot<commands.size();++slot) {
        const auto& command=commands[slot];
        if(command.firstInstance>=bounds.size() || command.instanceCount>1 ||
            surfaces[slot].instance!=command.firstInstance)
            throw std::invalid_argument("Surface instance mapping is invalid");
    }
}
std::vector<GpuSurfaceIdentity> VisibilityPrototype::reference(const GpuCullParameters& parameters) const {
    auto result=surfaces_;
    for(std::size_t slot=0;slot<commands_.size();++slot)
        result[slot].visible=referenceCullCommand(commands_[slot],bounds_[commands_[slot].firstInstance],parameters).instanceCount;
    return result;
}
std::optional<GpuSurfaceIdentity> VisibilityPrototype::pick(VisibilityFrameKey key,
    const std::vector<GpuSurfaceIdentity>& result,std::size_t slot) const {
    if(!(key==key_) || result.size()!=surfaces_.size())throw std::invalid_argument("Visibility result belongs to another frame");
    if(slot>=result.size())throw std::out_of_range("Surface pick slot out of range");
    // Validate the complete mapping before allowing any individual query.
    for(std::size_t index=0;index<result.size();++index) {
        const auto& expected=surfaces_[index];const auto& actual=result[index];
        if(actual.instance!=expected.instance || actual.primitive!=expected.primitive ||
            actual.material!=expected.material || actual.visible>1)
            throw std::invalid_argument("GPU surface identity mismatch");
    }
    return result[slot].visible?std::optional<GpuSurfaceIdentity>{result[slot]}:std::nullopt;
}
}
