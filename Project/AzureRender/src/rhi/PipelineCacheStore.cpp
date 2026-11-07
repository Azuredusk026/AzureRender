#include "rhi/PipelineCacheStore.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <regex>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace azurerender::rhi {
namespace {
constexpr std::size_t headerBytes=52;
constexpr std::array<std::uint8_t,8> magic{'A','Z','V','K','P','C','0','1'};
void put(PipelineCacheBytes& bytes,std::size_t at,std::uint64_t value,unsigned count){for(unsigned i=0;i<count;++i)bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));}
std::uint64_t get(const PipelineCacheBytes& bytes,std::size_t at,unsigned count){std::uint64_t value=0;for(unsigned i=0;i<count;++i)value|=std::uint64_t(bytes[at+i])<<(8*i);return value;}
std::uint64_t checksum(const std::uint8_t* data,std::size_t size){std::uint64_t hash=14695981039346656037ULL;for(std::size_t i=0;i<size;++i){hash^=data[i];hash*=1099511628211ULL;}return hash;}
bool payloadMatches(const PipelineCacheIdentity& id,const PipelineCacheBytes& payload){return payload.size()>=32
    &&get(payload,0,4)>=32&&get(payload,0,4)<=payload.size()&&get(payload,4,4)==1
    &&get(payload,8,4)==id.vendor&&get(payload,12,4)==id.device
    &&std::equal(id.uuid.begin(),id.uuid.end(),payload.begin()+16);}
std::string key(const PipelineCacheIdentity& id){std::ostringstream out;out<<"vk1-"<<std::hex<<id.vendor<<'-'<<id.device<<'-'<<id.driver<<'-'<<std::setfill('0');for(auto byte:id.uuid)out<<std::setw(2)<<static_cast<unsigned>(byte);return out.str()+".azpc";}
bool safePath(const std::filesystem::path& input){
    namespace fs=std::filesystem;
    for(auto path=input;!path.empty();path=path.parent_path()){
        std::error_code error;const auto status=fs::symlink_status(path,error);
        if(error&&error!=std::errc::no_such_file_or_directory)return false;
        if(fs::is_symlink(status))return false;
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
        if(path==path.root_path()||path==path.parent_path())break;
    }
    return true;
}
class PoolLock final {
public:
    explicit PoolLock(const std::filesystem::path& path){
#ifdef _WIN32
        handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle_!=INVALID_HANDLE_VALUE)locked_=LockFileEx(handle_,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&overlapped_)!=0;
#else
        handle_=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
        if(handle_>=0)locked_=::flock(handle_,LOCK_EX|LOCK_NB)==0;
#endif
    }
    ~PoolLock(){
#ifdef _WIN32
        if(locked_)UnlockFileEx(handle_,0,1,0,&overlapped_);
        if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);
#else
        if(locked_)::flock(handle_,LOCK_UN);
        if(handle_>=0)::close(handle_);
#endif
    }
    explicit operator bool()const noexcept{return locked_;}
    PoolLock(const PoolLock&)=delete;PoolLock& operator=(const PoolLock&)=delete;
private:
    bool locked_=false;
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;OVERLAPPED overlapped_{};
#else
    int handle_=-1;
#endif
};
bool owned(const std::filesystem::path& path){static const std::regex pattern("vk1-[0-9a-f]{1,8}-[0-9a-f]{1,8}-[0-9a-f]{1,8}-[0-9a-f]{32}\\.azpc");return std::regex_match(path.filename().string(),pattern);}
}

std::optional<PipelineCacheBytes> encodePipelineCache(const PipelineCacheIdentity& id,const PipelineCacheBytes& payload,std::uint64_t maximumBytes){
    if(maximumBytes<headerBytes||payload.size()>maximumBytes-headerBytes||!payloadMatches(id,payload))return {};
    PipelineCacheBytes bytes(headerBytes+payload.size());std::copy(magic.begin(),magic.end(),bytes.begin());
    put(bytes,8,id.vendor,4);put(bytes,12,id.device,4);put(bytes,16,id.driver,4);std::copy(id.uuid.begin(),id.uuid.end(),bytes.begin()+20);
    put(bytes,36,payload.size(),8);put(bytes,44,checksum(payload.data(),payload.size()),8);std::copy(payload.begin(),payload.end(),bytes.begin()+headerBytes);return bytes;
}
std::optional<PipelineCacheBytes> decodePipelineCache(const PipelineCacheIdentity& id,const PipelineCacheBytes& bytes,std::uint64_t maximumBytes){
    if(bytes.size()<headerBytes+32||bytes.size()>maximumBytes||!std::equal(magic.begin(),magic.end(),bytes.begin())
        ||get(bytes,8,4)!=id.vendor||get(bytes,12,4)!=id.device||get(bytes,16,4)!=id.driver
        ||!std::equal(id.uuid.begin(),id.uuid.end(),bytes.begin()+20)||get(bytes,36,8)!=bytes.size()-headerBytes
        ||get(bytes,44,8)!=checksum(bytes.data()+headerBytes,bytes.size()-headerBytes))return {};
    PipelineCacheBytes payload(bytes.begin()+headerBytes,bytes.end());if(!payloadMatches(id,payload))return {};return payload;
}
PipelineCacheStore::PipelineCacheStore(std::filesystem::path directory,PipelineCacheIdentity identity,PipelineCacheLimits limits)
    :directory_(directory.empty()?std::filesystem::path{}:std::filesystem::absolute(directory).lexically_normal()),identity_(identity),limits_(limits){}
std::filesystem::path PipelineCacheStore::file()const{return directory_.empty()?std::filesystem::path{}:directory_/key(identity_);}
PipelineCacheBytes PipelineCacheStore::load()const noexcept{try{
    const auto path=file();if(path.empty()||!safePath(path)||!std::filesystem::is_regular_file(path))return {};
    if(std::filesystem::file_time_type::clock::now()-std::filesystem::last_write_time(path)>std::chrono::seconds(limits_.retentionSeconds))return {};
    const auto size=std::filesystem::file_size(path);if(size<headerBytes+32||size>limits_.maximumFileBytes||size>std::numeric_limits<std::size_t>::max())return {};
    PipelineCacheBytes bytes(static_cast<std::size_t>(size));std::ifstream input(path,std::ios::binary);if(!input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size)))return {};
    auto result=decodePipelineCache(identity_,bytes,limits_.maximumFileBytes);return result?std::move(*result):PipelineCacheBytes{};
}catch(...){return {};}}
bool PipelineCacheStore::save(const PipelineCacheBytes& payload)const noexcept{try{
    namespace fs=std::filesystem;
    if(directory_.empty()||!limits_.maximumEntries||!safePath(directory_))return false;
    const auto encoded=encodePipelineCache(identity_,payload,limits_.maximumFileBytes);
    if(!encoded||encoded->size()>limits_.maximumPoolBytes)return false;
    fs::create_directories(directory_);const auto lockPath=directory_/".pool.lock";if(!safePath(lockPath))return false;
    PoolLock lock(lockPath);if(!lock)return false;
    const auto target=file(),temporary=fs::path(target.native()+fs::path(".tmp").native());
    if(!safePath(target)||!safePath(temporary))return false;
    struct Entry{fs::path path;std::uint64_t bytes;fs::file_time_type time;};std::vector<Entry> entries;
    std::vector<fs::directory_entry> candidates;
    for(const auto& entry:fs::directory_iterator(directory_)){if(candidates.size()==64)return false;candidates.push_back(entry);}
    const auto now=fs::file_time_type::clock::now();
    for(const auto& entry:candidates){
        const auto path=entry.path();
        const bool temp=path.extension()==".tmp"&&owned(path.stem());
        if(!owned(path)&&!temp)continue;
        if(!safePath(path)||!fs::is_regular_file(entry.symlink_status()))return false;
        const auto bytes=entry.file_size();const auto time=entry.last_write_time();
        if(temp||bytes>limits_.maximumFileBytes||(path!=target&&now-time>std::chrono::seconds(limits_.retentionSeconds))){if(!fs::remove(path))return false;continue;}
        entries.push_back({path,bytes,time});
    }
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.time<b.time;});
    std::uint64_t total=0,oldBytes=0;bool replacing=false;for(const auto& entry:entries){total+=entry.bytes;if(entry.path==target){oldBytes=entry.bytes;replacing=true;}}
    auto count=entries.size()+(replacing?0U:1U);
    for(const auto& entry:entries){
        if(total-oldBytes+encoded->size()<=limits_.maximumPoolBytes&&count<=limits_.maximumEntries)break;
        if(entry.path==target)continue;
        if(!fs::remove(entry.path))return false;total-=entry.bytes;--count;
    }
    if(total-oldBytes+encoded->size()>limits_.maximumPoolBytes||count>limits_.maximumEntries)return false;
    if(fs::space(directory_).available<encoded->size()+16ULL*1024*1024)return false;
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(encoded->data()),static_cast<std::streamsize>(encoded->size()));output.flush();if(!output){output.close();fs::remove(temporary);return false;}}
#ifdef _WIN32
    const bool stored=MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;fs::rename(temporary,target,error);const bool stored=!error;
#endif
    if(!stored){std::error_code error;fs::remove(temporary,error);}return stored;
}catch(...){return false;}}
} // namespace azurerender::rhi
