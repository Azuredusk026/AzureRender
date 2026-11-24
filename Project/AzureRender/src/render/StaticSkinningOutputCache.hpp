#pragma once
#include <cstdint>
#include <vector>
namespace azurerender {
struct StaticSkinningRange {
    std::uint32_t meshKey=0,firstVertex=0,vertexCount=0;
    bool operator==(const StaticSkinningRange& other)const noexcept {
        return meshKey==other.meshKey&&firstVertex==other.firstVertex&&vertexCount==other.vertexCount;
    }
};
// Content ownership belongs to one retired frame slot and its output buffer.
// The owner invalidates on allocation changes and commits only recorded work.
class StaticSkinningOutputCache {
    struct Entry {StaticSkinningRange range{};bool valid=false,pending=false;};
    std::vector<Entry> entries_;
    std::uint64_t generation_=0,lastSubmission_=0;
    std::size_t reused_=0;
    std::uint64_t reusedVertices_=0;
public:
    void begin(std::size_t count){reused_=0;reusedVertices_=0;++generation_;entries_.resize(count);for(auto& e:entries_)e.pending=false;}
    std::uint64_t generation()const noexcept{return generation_;}
    std::size_t reusedCount()const noexcept{return reused_;}
    std::uint64_t reusedVertexCount()const noexcept{return reusedVertices_;}
    bool prepareRange(std::size_t index,StaticSkinningRange range,bool eligible,bool consumed){
        auto& entry=entries_.at(index);
        if(!(entry.range==range)||!eligible)entry.valid=false;
        entry.range=range;
        if(consumed&&eligible&&entry.valid){++reused_;reusedVertices_+=range.vertexCount;}
        const bool write=consumed&&(!eligible||!entry.valid);
        entry.pending=write&&eligible;return write;
    }
    bool commit(std::uint64_t generation,std::uint64_t submission)noexcept{
        if(generation!=generation_){invalidate();return false;}
        if(!submission||submission<=lastSubmission_){cancel();return false;}
        for(auto& e:entries_){e.valid=e.valid||e.pending;e.pending=false;}
        lastSubmission_=submission;return true;
    }
    void cancel()noexcept{reused_=0;reusedVertices_=0;++generation_;for(auto& e:entries_)e.pending=false;}
    void invalidate()noexcept{reused_=0;reusedVertices_=0;++generation_;for(auto& e:entries_){e.valid=false;e.pending=false;}}
    void reset()noexcept{invalidate();entries_.clear();lastSubmission_=0;}
};
}
