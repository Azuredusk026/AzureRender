#include "render/MaterialOriginInstances.hpp"
#include <iostream>
#include <stdexcept>

using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Invalid material origin instance run must reject");}
}
int main(){try{
    std::vector<scene::SceneInstance> source(4);
    for(unsigned i=0;i<source.size();++i){source[i].sourceIndex=i;source[i].meshKey=i<3?0:1;}
    const MaterialOriginInstances defaults(source,{{},{}});
    require(defaults.gpuCount()==4&&defaults.copies().empty(),"Default materials must retain exactly the physical instance records");
    const MaterialOriginInstances plan(source,{{7,7,9},{7}});
    require(plan.gpuCount()==11&&plan.copies().size()==7,"Each source and unique origin node requires one reusable GPU record");
    require(plan.firstInstance(7,0,3)==4,"A material origin must retain a contiguous physical batch");
    require(plan.firstInstance(7,1,2)==5,"Opacity subruns must map to the same contiguous origin records");
    require(plan.firstInstance(9,0,3)==7,"Distinct origin nodes must select independent records");
    require(plan.firstInstance(7,3,1)==10,"Equal node indices in different mesh resources must keep distinct instance ownership");
    require(plan.copies()[0].sourceIndex==0&&plan.copies()[0].node==7&&plan.copies()[0].gpuIndex==4,
        "Copies must retain the physical source identity and selected node");
    rejects([&]{(void)plan.firstInstance(7,0,4);});
    rejects([&]{(void)plan.firstInstance(8,0,1);});
    rejects([&]{(void)plan.firstInstance(7,0,0);});
    rejects([&]{(void)plan.firstInstance(7,3,2);});
    auto interleaved=source;interleaved[1].meshKey=1;
    const MaterialOriginInstances mixed(interleaved,{{7},{9}});
    rejects([&]{(void)mixed.firstInstance(7,0,3);});
    auto invalid=source;invalid[2].sourceIndex=0;
    rejects([&]{MaterialOriginInstances bad(invalid,{{7},{7}});});
    rejects([&]{MaterialOriginInstances bad(source,{{7}});});
    std::cout<<"Material origin reuse, source ownership, contiguous batches and invalid runs passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
