#include "render/GpuPredicateDrawSelection.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void reject(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"Invalid draw selection accepted");}
}
int main()try{
 GpuPredicateSpan defaults;require(defaults.firstWord==0&&defaults.strideWords==0&&defaults.rejectMask==UINT32_MAX,"Shared predicate layout defaults preserve all rejection bits");
 rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
 auto source=backend.allocator().createBuffer(16*20,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
 auto predicate=backend.allocator().createBuffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
 const auto before=backend.allocator().statistics().liveBuffers;
 {
  GpuPredicateDrawSelection owner(backend,16,2,code);rhi::NullCommandRecorder commands;
  require(owner.bytes()==16*40*2&&owner.fastOffset()==16*20,"Bounded two-region frame output");
  auto first=owner.prepare(0,source,predicate,7);owner.record(commands,first);
  reject([&]{owner.record(commands,first);});
  auto next=owner.prepare(0,source,predicate,8,true);reject([&]{owner.record(commands,first);});owner.record(commands,next);
  owner.record(commands,owner.prepare(1,source,predicate,16,true));
  require(owner.output(0).buffer!=owner.output(1).buffer,"Independent frame outputs");
  {
   GpuPredicateDrawSelection left(backend,16,1,code),right(backend,16,1,code);
   const auto a=left.prepare(0,source,predicate,1),b=right.prepare(0,source,predicate,1);
   reject([&]{right.record(commands,a);});left.record(commands,a);right.record(commands,b);
  }
  reject([&]{owner.prepare(2,source,predicate,1,true);});
  reject([&]{owner.prepare(0,source,predicate,17,true);});
  auto shortSource=source;shortSource.size=19;reject([&]{owner.prepare(0,shortSource,predicate,1,true);});
  auto shortPredicate=predicate;shortPredicate.size=3;reject([&]{owner.prepare(0,source,shortPredicate,1,true);});
  reject([&]{owner.prepare(0,owner.output(0),predicate,1,true);});
  reject([&]{owner.prepare(0,source,owner.output(1),1,true);});
  reject([&]{owner.prepare(0,source,source,1,true);});
  reject([&]{owner.prepare(0,source,predicate,16,true,GpuPredicateSpan{4,1,1});});
  reject([&]{owner.prepare(0,source,predicate,1,true,GpuPredicateSpan{0,1,1});});
  reject([&]{owner.prepare(0,source,predicate,1,true,GpuPredicateSpan{0,0,0});});
  auto ranged=backend.allocator().createBuffer(4*(4+16*2),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
  owner.record(commands,owner.prepare(0,source,ranged,16,true,GpuPredicateSpan{4,2,1}));
  reject([&]{owner.prepare(0,source,ranged,16,true,GpuPredicateSpan{UINT32_MAX,UINT32_MAX,1});});
  backend.allocator().destroyBuffer(ranged);
  reject([&]{owner.prepare(0,source,predicate,1,true,GpuPredicateSpan{0,0,1,1});});
  auto empty=owner.prepare(0,{}, {},0);owner.record(commands,empty);require(empty.count==0,"Empty selection is a consumed no-op");
 }
 require(backend.allocator().statistics().liveBuffers==before,"Selection outputs retire");
 reject([&]{GpuPredicateDrawSelection owner(backend,0,2,code);});
 reject([&]{GpuPredicateDrawSelection owner(backend,1048576,8,code);});
 reject([&]{GpuPredicateDrawSelection owner(backend,16,0,code);});
 reject([&]{GpuPredicateDrawSelection owner(backend,16,2,{});});
 backend.allocator().destroyBuffer(source);backend.allocator().destroyBuffer(predicate);
 require(!backend.allocator().statistics().liveBuffers,"External inputs retire independently");
 std::cout<<"Predicate draw selection: ranges, fail-closed default, tickets and ownership passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
