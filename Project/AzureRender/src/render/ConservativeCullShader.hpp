#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
namespace azurerender {
// Structural capability v1: compute stage, false boolean SpecId0 and the
// 128-byte push block's final float/vec4 members at offsets108/112.
inline void validateConservativeCullShader(const std::vector<char>& bytes){
    const auto fail=[](){throw std::invalid_argument("Shader lacks conservative cull specialization/push ABI");};
    if(bytes.size()<20||bytes.size()%4||bytes.size()>16U*1024U*1024U)fail();
    std::vector<std::uint32_t> w(bytes.size()/4);std::memcpy(w.data(),bytes.data(),bytes.size());
    if(w[0]!=0x07230203U||!w[3]||w[4]||w[1]<0x10000||w[1]>0x10600)fail();
    std::set<unsigned> booleans,specZero;std::map<unsigned,unsigned> falseSpec,pointers,variables,structures;std::map<unsigned,std::map<unsigned,unsigned>> offsets;bool compute=false;
    for(std::size_t at=5;at<w.size();){const auto count=w[at]>>16,op=w[at]&0xffffU;if(!count||count>w.size()-at)fail();
        if(op==15&&count>=4&&w[at+1]==5)compute=true;
        if(op==20&&count==2)booleans.insert(w[at+1]);
        if(op==49&&count==3)falseSpec[w[at+2]]=w[at+1];
        if(op==71&&count==4&&w[at+2]==1&&w[at+3]==0)specZero.insert(w[at+1]);
        if(op==30&&count>=2)structures[w[at+1]]=count-2;
        if(op==32&&count==4&&w[at+2]==9)pointers[w[at+1]]=w[at+3];
        if(op==59&&count>=4&&w[at+3]==9)variables[w[at+2]]=w[at+1];
        if(op==72&&count==5&&w[at+3]==35)offsets[w[at+1]][w[at+2]]=w[at+4];at+=count;
    }
    bool feature=false,push=false;for(auto id:specZero)if(falseSpec.count(id)&&booleans.count(falseSpec.at(id)))feature=true;
    for(const auto& entry:variables)if(pointers.count(entry.second)){const auto id=pointers.at(entry.second);if(structures[id]==6&&offsets[id][4]==108&&offsets[id][5]==112)push=true;}
    if(!compute||!feature||!push)fail();
}
}
