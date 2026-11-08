#include "rhi/PipelineCacheStore.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender::rhi;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(PipelineCacheBytes& data,std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4;++i)data[offset+i]=static_cast<std::uint8_t>(value>>(i*8));}
PipelineCacheBytes payload(const PipelineCacheIdentity& id){PipelineCacheBytes data(64,7);put(data,0,32);put(data,4,1);put(data,8,id.vendor);put(data,12,id.device);std::copy(id.uuid.begin(),id.uuid.end(),data.begin()+16);return data;}
void write(const std::filesystem::path& path,const PipelineCacheBytes& bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!out)throw std::runtime_error("Fixture write failed");}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Expected dedicated test-artifacts path");
    const auto root=std::filesystem::absolute(argv[1]).lexically_normal();
    require(root.filename()=="pipeline-cache-store"&&root.parent_path().filename()=="test-artifacts","Expected dedicated test-artifacts child");
    std::filesystem::create_directories(root);
    const auto shaders=root/"shaders";std::filesystem::create_directories(shaders);
    write(shaders/"a.spv",{1,2,3,4});write(shaders/"b.spv",{5,6,7,8});
    const auto profile=shaderCacheProfile(shaders);
    require(profile&&*profile!=0,"A bounded shader set must have a persistent profile");
    const auto moved=root/"moved";std::filesystem::create_directories(moved);
    std::filesystem::remove(moved/"c.spv");
    write(moved/"b.spv",{5,6,7,8});write(moved/"a.spv",{1,2,3,4});
    std::ofstream(moved/"note.txt")<<"unrelated";
    require(shaderCacheProfile(moved)==profile,"Profile must depend on names and bytes, not location or enumeration order");
    write(moved/"a.spv",{1,2,3,5});require(shaderCacheProfile(moved)!=profile,"Program changes must select a fresh generation");
    std::filesystem::rename(moved/"a.spv",moved/"c.spv");
    const auto renamed=shaderCacheProfile(moved);require(renamed&&renamed!=profile,"Program names must be included");
    require(!shaderCacheProfile(root/"missing"),"Unavailable programs must disable persistent generation reuse");
    const auto oversized=root/"oversized";std::filesystem::create_directories(oversized);
    write(oversized/"large.spv",{0});std::filesystem::resize_file(oversized/"large.spv",16ULL*1024*1024+1);
    require(!shaderCacheProfile(oversized),"Oversized programs must stop bounded inspection");
    PipelineCacheIdentity id;id.vendor=0x10de;id.device=0x28a0;id.driver=17;
    for(unsigned i=0;i<16;++i)id.uuid[i]=static_cast<std::uint8_t>(i);
    const auto data=payload(id);auto encoded=encodePipelineCache(id,data,512);
    auto profiled=id;profiled.profile=*profile;
    PipelineCacheStore generation(root/"generations",profiled);
    PipelineCacheStore legacy(root/"generations",id);
    require(generation.file()!=legacy.file(),"Program generations must have independent cache files");
    require(generation.save(data)&&generation.load()==data&&legacy.load().empty(),"Fresh generations must persist independently");
    PipelineCacheLimits oneGeneration;oneGeneration.maximumEntries=1;
    auto next=profiled;++next.profile;
    PipelineCacheStore replacement(root/"generations",next,oneGeneration);
    require(replacement.save(data)&&!std::filesystem::exists(generation.file()),"Generations must share entry and pool limits");
    require(encoded.has_value(),"Valid driver payload must encode");
    auto decoded=decodePipelineCache(id,*encoded,512);require(decoded&&*decoded==data,"Cache bytes must round-trip exactly");
    auto corrupted=*encoded;corrupted.back()^=1;require(!decodePipelineCache(id,corrupted,512),"Payload damage must reject before driver use");
    corrupted=*encoded;corrupted.pop_back();require(!decodePipelineCache(id,corrupted,512),"Truncated caches must reject");
    require(!decodePipelineCache(id,*encoded,encoded->size()-1),"Read budgets must reject oversized files");
    auto other=id;++other.driver;require(!decodePipelineCache(other,*encoded,512),"Driver changes must reject cached data");
    other=id;other.uuid[0]^=1;require(!decodePipelineCache(other,*encoded,512),"Cache UUID changes must reject data");
    other=id;++other.device;require(!encodePipelineCache(other,data,512),"Payload and identity must refer to one device");
    PipelineCacheLimits limits;limits.maximumFileBytes=512;limits.maximumPoolBytes=240;limits.maximumEntries=2;limits.retentionSeconds=3600;
    const auto pool=root/"pool";PipelineCacheStore first(pool,id,limits);
    require(first.save(data),"First cache must persist");require(first.load()==data,"Stored cache must load its original payload");
    write(first.file(),corrupted);require(first.load().empty(),"Broken persisted cache must use an empty fallback");require(first.save(data),"A rebuilt cache must replace its broken predecessor");
    other=id;++other.driver;PipelineCacheStore second(pool,other,limits);require(second.save(payload(other)),"Second cache must persist");
    std::filesystem::last_write_time(first.file(),std::filesystem::file_time_type::clock::now()-std::chrono::seconds(60));
    std::ofstream(pool/"authored.txt")<<"owned test control";
    std::ofstream(pool/"vk1-not-an-engine-cache.azpc")<<"owned test control";
    ++other.driver;PipelineCacheStore third(pool,other,limits);require(third.save(payload(other)),"Pool must evict an old rebuildable entry");
    require(!std::filesystem::exists(first.file())&&second.load()==payload(PipelineCacheIdentity{id.vendor,id.device,id.driver+1,id.uuid}),"Pool eviction must preserve the recent cache");
    require(std::filesystem::exists(pool/"authored.txt"),"Foreign files must remain untouched");
    require(std::filesystem::exists(pool/"vk1-not-an-engine-cache.azpc"),"Lookalike foreign filenames must remain untouched");
    std::filesystem::last_write_time(second.file(),std::filesystem::file_time_type::clock::now()-std::chrono::seconds(7200));
    require(third.save(payload(other))&&!std::filesystem::exists(second.file()),"Expired entries must be reclaimed");
    auto tooLarge=data;tooLarge.resize(500);require(!third.save(tooLarge)&&third.load()==payload(other),"Rejected writes must preserve the current cache");
    PipelineCacheStore disabled({},id,limits);require(disabled.load().empty()&&!disabled.save(data),"An empty cache directory must have no persistent writes");
    std::cout<<"Pipeline cache identity, corruption, limits, expiry, foreign files and fallback passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
