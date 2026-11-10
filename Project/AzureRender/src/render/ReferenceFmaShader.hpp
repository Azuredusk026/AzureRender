#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace azurerender {
struct ReferenceFmaShader {std::vector<char> code;std::uint32_t fusedResults=0;};
// A marked local value has one direct Fma definition. Move its explicit
// NoContraction annotation to that arithmetic result before module creation.
// Source compilation and hot reload use the same explicit preparation call.
inline ReferenceFmaShader prepareReferenceFmaShader(const std::vector<char>& input){
    const auto reject=[](){throw std::invalid_argument("Invalid or unmarked reference Fma SPIR-V");};
    if(input.size()<20||input.size()%4||input.size()>16U*1024U*1024U)reject();
    std::vector<std::uint32_t> words(input.size()/4);std::memcpy(words.data(),input.data(),input.size());
    if(words[0]!=0x07230203U||words[3]==0||words[4]!=0||words[1]<0x00010000U||words[1]>0x00010600U)reject();
    struct Instruction {std::size_t at;std::uint32_t count,op;};std::vector<Instruction> instructions;
    std::set<std::uint32_t> imports,variables,decorated,fma;std::map<std::uint32_t,std::vector<std::uint32_t>> stores;
    std::size_t types=0;
    for(std::size_t at=5;at<words.size();){
        const auto count=words[at]>>16,op=words[at]&0xffffU;
        if(!count||count>words.size()-at)reject();instructions.push_back({at,count,op});
        if(op==11){
            if(count<3||words[at+1]>=words[3])reject();const auto data=reinterpret_cast<const char*>(words.data()+at+2);const auto size=(count-2)*4;
            const auto end=static_cast<const char*>(std::memchr(data,0,size));if(!end)reject();
            if(std::string(data,end)=="GLSL.std.450")imports.insert(words[at+1]);
        }
        if(op==59){if(count<4)reject();variables.insert(words[at+2]);}
        if(op==71){if(count<3)reject();if(words[at+2]==42)decorated.insert(words[at+1]);}
        if(op==62){if(count<3)reject();stores[words[at+1]].push_back(words[at+2]);}
        if(op==19&&!types)types=at;at+=count;
    }
    for(const auto& instruction:instructions)if(instruction.op==12){
        const auto at=instruction.at;if(instruction.count<5)reject();
        if(imports.count(words[at+3])&&words[at+4]==50){if(instruction.count!=8||words[at+2]>=words[3])reject();fma.insert(words[at+2]);}
    }
    std::set<std::uint32_t> marked,results;
    for(auto id:decorated){
        if(fma.count(id))results.insert(id);
        if(!variables.count(id))continue;
        const auto found=stores.find(id);
        if(found==stores.end()||found->second.size()!=1||!fma.count(found->second.front()))reject();
        marked.insert(id);results.insert(found->second.front());
    }
    if(results.empty()||results.size()>128||!types)reject();
    ReferenceFmaShader prepared;prepared.fusedResults=static_cast<std::uint32_t>(results.size());
    if(marked.empty()){prepared.code=input;return prepared;}
    std::vector<std::uint32_t> output(words.begin(),words.begin()+5);bool inserted=false;
    for(const auto& instruction:instructions){
        const auto at=instruction.at;
        if(at==types){
            for(auto id:results)if(!decorated.count(id)){output.push_back((3U<<16)|71U);output.push_back(id);output.push_back(42);}
            inserted=true;
        }
        if(instruction.op==71&&instruction.count==3&&words[at+2]==42&&marked.count(words[at+1]))continue;
        output.insert(output.end(),words.begin()+at,words.begin()+at+instruction.count);
    }
    if(!inserted)reject();prepared.code.resize(output.size()*4);std::memcpy(prepared.code.data(),output.data(),prepared.code.size());return prepared;
}
}
