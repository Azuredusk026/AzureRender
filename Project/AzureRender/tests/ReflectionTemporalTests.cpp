#include "render/ReflectionTemporal.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void require(bool v){if(!v)throw std::runtime_error("Temporal reflection contract failed");}
template<class F>void rejects(F op){bool failed=false;try{op();}catch(const std::exception&){failed=true;}require(failed);}
int main()try{
    ReflectionTemporalParameters parameters;const auto frame=parameters.frame(17,9);static_assert(sizeof(frame)==32);
    require(frame[0][0]==17&&frame[0][1]==9&&frame[1][0]==1.25F&&frame[1][1]==.97F&&frame[1][3]==2);
    parameters.historyValid=true;require(parameters.frame(17,9)[1][3]==3);
    parameters.rejectOutside=false;require(parameters.frame(17,9)[1][3]==1);
    rejects([&]{parameters.frame(0,9);});rejects([&]{parameters.frame(9000,9);});
    parameters.historyWeight=1.01F;rejects([&]{parameters.frame(17,9);});parameters.historyWeight=.97F;
    parameters.varianceScale=-1;rejects([&]{parameters.frame(17,9);});parameters.varianceScale=1.25F;
    parameters.minimumColor=std::numeric_limits<float>::quiet_NaN();rejects([&]{parameters.frame(17,9);});parameters.minimumColor=.001F;
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    ReflectionTemporalShaders shaders{code,code};
    rejects([&]{ReflectionTemporalGpu p(backend,{17,9},0,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);});
    rejects([&]{ReflectionTemporalGpu p(backend,{17,9},2,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);});
    {
        ReflectionSignalPyramidGpu signals(backend,{17,9},1,2,ReflectionPyramidPrecision::Float32,{code,code});
        ReflectionTemporalGpu p(backend,{17,9},2,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,shaders);
        const auto view=[](unsigned n){return reinterpret_cast<VkImageView>(static_cast<std::uintptr_t>(n));};
        ReflectionTemporalInputs input{view(1000),view(1001),view(1002),view(1003),view(1004),view(1005),17,9};
        rhi::NullCommandRecorder commands;const auto writes=backend.descriptorImageWrites.size();
        auto bad=input;bad.previousColor=signals.base(0).color;rejects([&]{p.record(commands,0,bad,signals.base(0),parameters);});
        bad=input;bad.weight=VK_NULL_HANDLE;rejects([&]{p.record(commands,0,bad,signals.base(0),parameters);});
        bad=input;bad.width=16;rejects([&]{p.record(commands,0,bad,signals.base(0),parameters);});
        auto output=signals.base(0);output.levels=2;rejects([&]{p.record(commands,0,input,output,parameters);});
        require(commands.calls.empty()&&writes==backend.descriptorImageWrites.size());
        signals.beginBaseWrite(commands,0);p.record(commands,0,input,signals.base(0),parameters);
        require(!commands.calls.empty());
    }
    require(!backend.allocator().statistics().liveBuffers&&!backend.allocator().statistics().liveImages);
    std::cout<<"PASS: temporal frame, bounds, aliases, borrowed outputs and retirement\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
