#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
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
        samples_.reserve(capacity);
    }
    void record(const ResourceFrameSample& value){
        if(samples_.size()<capacity_)samples_.push_back(value);
        else {samples_[next_]=value;next_=(next_+1)%capacity_;}
    }
    void write(std::ostream& output)const{
        output<<'[';
        const auto first=samples_.size()==capacity_?next_:0;
        for(std::size_t index=0;index<samples_.size();++index){
            if(index)output<<',';
            const auto& value=samples_[(first+index)%samples_.size()];
            const nlohmann::json row={{"frame",value.frame},{"revision",value.revision},
            {"workMs",value.workMs},{"waitMs",value.waitMs},{"physicsMaxMs",value.physicsMaxMs},
            {"committed",value.committed},{"commitMs",value.commitMs},{"loading",value.loading},
            {"cachedCandidates",value.cachedCandidates},{"requestGeneration",value.requestGeneration},
            {"loadError",value.loadError},{"buffers",value.buffers},{"images",value.images},
            {"bufferBytes",value.bufferBytes},{"imageBytes",value.imageBytes}};
            output<<row.dump();
        }
        output<<']';
    }
private:
    std::size_t capacity_,next_=0;
    std::vector<ResourceFrameSample> samples_;
};
}
