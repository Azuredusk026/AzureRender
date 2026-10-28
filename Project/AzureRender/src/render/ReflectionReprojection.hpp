#pragma once
#include "rhi/Rhi.hpp"
#include <array>
#include <vector>
namespace azurerender {
using ReflectionReprojectionFrame=std::array<std::array<float,4>,3>;
static_assert(sizeof(ReflectionReprojectionFrame)==48);
struct ReflectionReprojectionParameters {
    // P[10], P[14], P[11], P[15] map positive view depth to device depth.
    std::array<float,4> previousDepthProjection{-1,-.1F,-1,0};
    float absoluteDepthTolerance=.001F,relativeDepthTolerance=.02F;
    bool historyValid=false;
    ReflectionReprojectionFrame frame(VkExtent2D source,VkExtent2D output) const;
};
struct ReflectionReprojectionInputs {
    VkImageView surface{},motion{},previousSelectedMotion{};
    unsigned width=0,height=0;
};
struct ReflectionReprojectionOutput {
    VkImage motionImage{},acceptanceImage{};
    VkImageView motion{},acceptance{};
    VkExtent2D extent{};
};
// Sources are borrowed sampled views. Outputs become sampled after record.
// The caller owns successful-submission history identity and retires slot consumers.
class ReflectionReprojectionGpu final {
public:
    static constexpr std::uint64_t kBytesPerPixel=20,kMaximumBytes=256ULL*1024*1024;
    ReflectionReprojectionGpu(rhi::IRhi&,VkExtent2D source,unsigned frames,const std::vector<char>& shader,unsigned resolutionDivisor=1);
    ~ReflectionReprojectionGpu();
    ReflectionReprojectionGpu(const ReflectionReprojectionGpu&)=delete;
    ReflectionReprojectionGpu& operator=(const ReflectionReprojectionGpu&)=delete;
    std::uint64_t byteSize() const noexcept{return bytes_;}
    ReflectionReprojectionOutput output(unsigned slot) const;
    void record(rhi::ICommandRecorder&,unsigned slot,const ReflectionReprojectionInputs&,const ReflectionReprojectionParameters&);
private:
    struct Slot {rhi::GpuImage motion{},acceptance{};VkImageView motionView{},acceptanceView{};rhi::GpuBuffer parameters{};VkDescriptorSet set{};};
    void release() noexcept;
    rhi::IRhi& backend_;VkExtent2D source_{},extent_{};std::uint64_t bytes_=0;
    std::vector<Slot> slots_;VkSampler point_{};VkDescriptorSetLayout descriptors_{};VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};VkShaderModule shader_{};VkPipeline pipeline_{};
};
}
