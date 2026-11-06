#include "render/StableDepthOrder.hpp"
#include <random>
#include <numeric>
#include <iostream>
#include <limits>
int main(){try{
 std::mt19937 random(7331);std::size_t cases=0;
 for(std::size_t count:{0U,1U,2U,31U,255U,256U,257U,4096U,32768U,65536U})for(unsigned shape=0;shape<6;++shape){
  std::vector<float> depth(count);std::vector<std::uint32_t> order(count);std::iota(order.begin(),order.end(),0U);
  for(std::size_t i=0;i<count;++i){std::uint32_t bits=random()&0xfeffffffU;std::memcpy(&depth[i],&bits,4);
   if(shape==1)depth[i]=float(i%13)-6;
   if(shape==2)depth[i]=i%2?0.F:-0.F;
   if(shape==3)depth[i]=float(count-i);
   if(shape==4)depth[i]=float(i);
   if(shape==5)depth[i]=std::numeric_limits<float>::quiet_NaN();
  }
  std::shuffle(order.begin(),order.end(),random);auto expected=order;const auto original=depth;
  std::stable_sort(expected.begin(),expected.end(),[&](auto x,auto y){return depth[x]>depth[y];});azurerender::stableDepthOrder(order,depth);
  if(order!=expected||(!depth.empty()&&std::memcmp(original.data(),depth.data(),depth.size()*4)))throw std::runtime_error("Stable depth order must match its reference comparator");++cases;
 }

 {
  const std::vector<float> limits={0.F,-0.F,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min()};
  std::vector<float> depth(1024);std::vector<std::uint32_t> order(1024);std::iota(order.begin(),order.end(),0U);
  for(std::size_t i=0;i<depth.size();++i)depth[i]=limits[i%limits.size()];std::shuffle(order.begin(),order.end(),random);auto expected=order;
  std::stable_sort(expected.begin(),expected.end(),[&](auto x,auto y){return depth[x]>depth[y];});azurerender::stableDepthOrder(order,depth);
  if(order!=expected)throw std::runtime_error("Infinite, signed-zero and subnormal keys must retain comparator order");
 }
 std::vector<std::uint32_t> bad{0,2};const auto prior=bad;bool rejected=false;try{azurerender::stableDepthOrder(bad,{1,2});}catch(const std::out_of_range&){rejected=true;}
 if(!rejected||bad!=prior)throw std::runtime_error("Invalid depth index must reject without mutation");std::cout<<"Stable depth oracle passed "<<cases<<" distributions and invalid input\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
