#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
namespace azurerender {
struct ResourceFrameSample {
    std::uint64_t frame=0,revision=0;
    double workMs=0,waitMs=0,physicsMaxMs=0;
    bool committed=false;
    double commitMs=0;
    bool loading=false;
    std::uint64_t cachedCandidates=0,requestGeneration=0;
    std::string loadError;
    std::uint64_t buffers=0,images=0,bufferBytes=0,imageBytes=0;
};
class ResourceFrameTrace {
public:
    explicit ResourceFrameTrace(std::size_t capacity=8192):capacity_(capacity){
        if(!capacity || capacity>8192)throw std::invalid_argument("Resource trace capacity must be between one and 8192");
    }
    void record(const ResourceFrameSample& value){
        if(samples_.size()==capacity_)samples_.erase(samples_.begin());
        samples_.push_back({{"frame",value.frame},{"revision",value.revision},
            {"workMs",value.workMs},{"waitMs",value.waitMs},{"physicsMaxMs",value.physicsMaxMs},
            {"committed",value.committed},{"commitMs",value.commitMs},{"loading",value.loading},
            {"cachedCandidates",value.cachedCandidates},{"requestGeneration",value.requestGeneration},
            {"loadError",value.loadError},{"buffers",value.buffers},{"images",value.images},
            {"bufferBytes",value.bufferBytes},{"imageBytes",value.imageBytes}});
    }
    void write(std::ostream& output)const{output<<samples_.dump();}
private:
    std::size_t capacity_;
    nlohmann::json samples_=nlohmann::json::array();
};
}
