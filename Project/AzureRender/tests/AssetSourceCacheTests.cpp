#include "runtime/AssetSourceCache.hpp"
#include "runtime/AssetDatabase.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace azurerender;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::uint64_t contentHash(std::string_view value){std::uint64_t h=14695981039346656037ULL;for(unsigned char c:value){h^=c;h*=1099511628211ULL;}return h;}
void set64(std::string& data,std::size_t at,std::uint64_t value){for(unsigned i=0;i<8;++i)data.at(at+i)=static_cast<char>(value>>(8*i));}
void verifySnapshotStorage(){
    namespace fs=std::filesystem;
    struct Fixture {fs::path root;bool owned=false;~Fixture(){if(owned){std::error_code error;fs::remove_all(root,error);}}};
    const auto parent=fs::weakly_canonical(fs::temp_directory_path());
    Fixture fixture{parent/("azure_source_cache-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    check(fixture.root.parent_path()==parent,"Snapshot fixture stays in its temporary parent");
    fixture.owned=fs::create_directory(fixture.root);check(fixture.owned,"Snapshot fixture owns one unique temporary child");
    const auto write=[](const fs::path& path,const std::string& value){std::ofstream file(path,std::ios::binary);file.write(value.data(),value.size());file.close();check(bool(file),"Snapshot fixture write failed");};
    const auto read=[](const fs::path& path){std::ifstream file(path,std::ios::binary);check(bool(file),"Snapshot fixture read failed");return std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());};
    Project::create(fixture.root,"Snapshot storage");const auto project=Project::load(fixture.root/"project.azureproject");
    const std::string first(1024*1024,'a');auto second=first;second.front()='b';
    const auto source=fixture.root/"assets/geometry.bin";write(source,first);
    AssetDatabase db(project);db.refresh();const auto id=db.idForPath("assets:/geometry.bin");
    check(read(db.cacheFile(id)).substr(0,8)=="AZCACHE2"&&fs::file_size(db.cacheFile(id))<8192,"Database stores large compressible snapshots compactly");
    AssetDatabase previous=db;write(source,second);db.refresh(true);
    check(previous.readSource(id)==first&&db.readSource(id)==second,"Refresh preserves old snapshot bytes after source mutation");
    const auto record=db.records().at(id);std::string legacy(24,'\0');legacy.replace(0,8,"AZCACHE1");set64(legacy,8,record.fingerprint);set64(legacy,16,second.size());legacy+=second;write(db.cacheFile(id),legacy);
    const auto alias=fixture.root/"shared-cache.bin";fs::create_hard_link(db.cacheFile(id),alias);
    AssetDatabase shared(project);shared.refresh();
    check(read(shared.cacheFile(id))==legacy&&fs::hard_link_count(alias)==2,"Shared legacy storage stays shared while another name owns its physical bytes");
    check(fs::remove(alias),"Fixture retires its own shared cache name");
    AssetDatabase migrated(project);migrated.refresh();const auto compact=read(migrated.cacheFile(id));
    check(compact.substr(0,8)=="AZCACHE2"&&migrated.readSource(id)==second,"Refresh migrates a matching large legacy snapshot");
    const auto stamp=fs::file_time_type::clock::now()-std::chrono::hours(24);fs::last_write_time(migrated.cacheFile(id),stamp);
    const auto actualStamp=fs::last_write_time(migrated.cacheFile(id));AssetDatabase warm(project);warm.refresh();
    check(fs::last_write_time(warm.cacheFile(id))==actualStamp&&read(warm.cacheFile(id))==compact,"Warm refresh preserves the complete cached file");
    auto corrupt=compact;corrupt.back()^=1;write(warm.cacheFile(id),corrupt);
    check(warm.readSource(id)==second&&read(warm.cacheFile(id))==compact,"Damaged compressed snapshots rebuild from matching sources");
    check(fs::remove(warm.cacheFile(id)),"Fixture removes only its generated snapshot");
    std::vector<std::future<void>> publishers;
    for(unsigned i=0;i<3;++i)publishers.push_back(std::async(std::launch::async,[&]{AssetDatabase candidate(project);candidate.refresh();check(candidate.readSource(id)==second,"Concurrent snapshot publisher returned different bytes");}));
    for(auto& job:publishers)job.get();check(read(warm.cacheFile(id))==compact,"Concurrent publishers preserve one complete compressed envelope");
    db.writePack(fixture.root/"pack");check(read(AssetDatabase::resolvePack(fixture.root/"pack",id))==second,"Packing resolves the original source bytes from compressed snapshots");
    const auto moved=fixture.root.string()+"-moved";check(fs::path(moved).parent_path()==parent,"Moved fixture remains in its temporary parent");
    fs::rename(fixture.root,moved);fixture.root=moved;
    AssetDatabase relocated(Project::load(fixture.root/"project.azureproject"));relocated.refresh();
    check(relocated.idForPath("assets:/geometry.bin")==id&&relocated.readSource(id)==second,"Moved projects retain asset identity and compressed snapshot contents");
}
int main(int argc,char** argv)try{
    if(argc>1){
        check(std::string(argv[1])=="--corpus"&&argc>2,"Expected --corpus and explicit source files");
        nlohmann::json rows=nlohmann::json::array();
        for(int i=2;i<argc;++i){
            const auto path=std::filesystem::u8path(argv[i]);std::ifstream file(path,std::ios::binary|std::ios::ate);check(bool(file),"Cannot open corpus input");
            const auto bytes=file.tellg();check(bytes>=0&&bytes<=static_cast<std::streamoff>(kAssetCacheCompressionMaximum),"Corpus input exceeds codec bounds");
            std::string source(static_cast<std::size_t>(bytes),'\0');file.seekg(0);file.read(source.data(),source.size());check(bool(file),"Cannot read corpus input");
            const auto hash=contentHash(source);auto start=std::chrono::steady_clock::now();auto encoded=encodeAssetSourceCache(source,123);
            const auto encodeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();start=std::chrono::steady_clock::now();
            const auto decoded=decodeAssetSourceCache(encoded,123,hash);const auto decodeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            check(decoded&&*decoded==source,"Corpus snapshot bytes differ");
            start=std::chrono::steady_clock::now();check(assetSourceCacheMatches(encoded,123,source),"Warm cache source bytes differ");
            const auto matchMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            rows.push_back({{"path",path.u8string()},{"sourceBytes",source.size()},{"legacyBytes",source.size()+24},{"encodedBytes",encoded.size()},{"format",encoded.substr(0,8)},{"encodeMs",encodeMs},{"decodeMs",decodeMs},{"warmMatchMs",matchMs},{"roundTripExact",true}});
        }
        std::cout<<rows.dump(2)<<'\n';return 0;
    }
    check(decodeAssetSourceCache(encodeAssetSourceCache(std::string_view{},37),37,contentHash({}))==std::string{},"A default empty view is a valid source");
    for(const auto& source:std::vector<std::string>{"",std::string("a\0b",3),std::string(65535,'x')}){
        auto envelope=encodeAssetSourceCache(source,37);check(envelope.substr(0,8)=="AZCACHE1","Small snapshots retain version 1");
        check(envelope.size()==24+source.size(),"Version 1 preserves its exact envelope length");
        check(decodeAssetSourceCache(envelope,37,contentHash(source))==source,"Legacy snapshots round-trip exact source bytes");
        check(assetSourceCacheMatches(envelope,37,source)&&!assetSourceCacheMatches(envelope,37,source+"x"),"Raw warm validation compares exact source bytes");
    }
    const std::string source(2*1024*1024,'v');const auto hash=contentHash(source);
    auto envelope=encodeAssetSourceCache(source,91);
    check(envelope.substr(0,8)=="AZCACHE2"&&envelope.size()<source.size()/8,"Large repetitive snapshots use bounded compression");
    check(decodeAssetSourceCache(envelope,91,hash)==source,"Compressed snapshots round-trip byte-exactly");
    check(assetSourceCacheMatches(envelope,91,source)&&!assetSourceCacheMatches(envelope,92,source),"Compressed warm validation checks identity and exact bytes");
    auto changedSource=source;changedSource.back()='w';check(!assetSourceCacheMatches(envelope,91,changedSource),"Warm validation rejects changed source contents");
    check(!decodeAssetSourceCache(envelope,92,hash),"Foreign fingerprints must miss");
    check(!decodeAssetSourceCache(envelope,91,hash+1),"Foreign content hashes must miss");
    auto damaged=envelope;damaged.back()^=1;check(!decodeAssetSourceCache(damaged,91,hash),"Corrupt compressed frames must miss");
    damaged=envelope;damaged.resize(damaged.size()-1);check(!decodeAssetSourceCache(damaged,91,hash),"Truncated frames must miss");
    damaged=envelope;damaged.push_back('x');check(!decodeAssetSourceCache(damaged,91,hash),"Trailing bytes must miss");
    damaged=envelope;set64(damaged,16,UINT64_MAX);check(!decodeAssetSourceCache(damaged,91,hash),"Oversized declarations must fail before output allocation");
    damaged=envelope;set64(damaged,24,UINT64_MAX);check(!decodeAssetSourceCache(damaged,91,hash),"Oversized payload declarations must miss");
    damaged=envelope;set64(damaged,32,99);check(!decodeAssetSourceCache(damaged,91,hash),"Unknown storage methods must miss");
    damaged=envelope;damaged[44]=static_cast<char>(static_cast<unsigned char>(damaged[44])&~4U);check(!decodeAssetSourceCache(damaged,91,hash),"Compressed snapshots require a frame checksum");
    if((static_cast<unsigned char>(envelope[44])&32U)==0){damaged=envelope;damaged[45]=static_cast<char>(255);check(!decodeAssetSourceCache(damaged,91,hash),"Oversized declared windows must miss before output allocation");}
    damaged=envelope+envelope.substr(40);set64(damaged,24,damaged.size()-40);check(!decodeAssetSourceCache(damaged,91,hash),"Concatenated compressed frames must miss");
    std::string random(256*1024,'\0');std::uint32_t state=217;
    for(auto& byte:random){state^=state<<13;state^=state>>17;state^=state<<5;byte=static_cast<char>(state);}
    auto raw=encodeAssetSourceCache(random,93);
    check(raw.substr(0,8)=="AZCACHE2"&&raw.size()==random.size()+40,"Incompressible snapshots record an uncompressed version 2 decision");
    check(decodeAssetSourceCache(raw,93,contentHash(random))==random,"Raw version 2 snapshots round-trip");
    for(std::size_t i=0;i<40;++i)check(!decodeAssetSourceCache(envelope.substr(0,i),91,hash),"Incomplete headers must miss");
    verifySnapshotStorage();
    std::cout<<"Asset source snapshot encoding, integrity and bounds passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
