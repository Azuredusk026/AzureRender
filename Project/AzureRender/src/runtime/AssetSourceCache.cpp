#include "runtime/AssetSourceCache.hpp"
#include <zstd.h>
#include <zstd_errors.h>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
namespace azurerender {
namespace {
void append64(std::string& out,std::uint64_t value){for(unsigned i=0;i<8;++i)out.push_back(static_cast<char>(value>>(8*i)));}
void put64(std::string& out,std::size_t at,std::uint64_t value){for(unsigned i=0;i<8;++i)out[at+i]=static_cast<char>(value>>(8*i));}
std::uint64_t read64(std::string_view value,std::size_t at){std::uint64_t result=0;for(unsigned i=0;i<8;++i)result|=std::uint64_t(static_cast<unsigned char>(value[at+i]))<<(8*i);return result;}
std::uint64_t hash(std::string_view value){std::uint64_t result=14695981039346656037ULL;for(unsigned char byte:value){result^=byte;result*=1099511628211ULL;}return result;}
void checked(std::size_t code){if(ZSTD_isError(code))throw std::runtime_error(std::string("Asset cache compression failed: ")+ZSTD_getErrorName(code));}
}
std::string encodeAssetSourceCache(std::string_view source,std::uint64_t fingerprint){
    if(source.size()<kAssetCacheCompressionMinimum||source.size()>kAssetCacheCompressionMaximum){
        std::string out="AZCACHE1";append64(out,fingerprint);append64(out,source.size());if(!source.empty())out.append(source.data(),source.size());return out;
    }
    std::unique_ptr<ZSTD_CCtx,decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(),ZSTD_freeCCtx);
    if(!context)throw std::bad_alloc();
    checked(ZSTD_CCtx_setParameter(context.get(),ZSTD_c_compressionLevel,1));
    checked(ZSTD_CCtx_setParameter(context.get(),ZSTD_c_checksumFlag,1));
    // One output allocation also holds the raw fallback. Compression stops at
    // the size that provides the declared on-disk saving, avoiding an oversized
    // temporary compressed buffer followed by a second payload copy.
    std::string out(40+source.size(),'\0');std::memcpy(out.data(),"AZCACHE2",8);
    const auto capacity=(source.size()+24)*7/8-40;
    const auto size=ZSTD_compress2(context.get(),out.data()+40,capacity,source.data(),source.size());
    const bool compressed=!ZSTD_isError(size);
    if(!compressed&&ZSTD_getErrorCode(size)!=ZSTD_error_dstSize_tooSmall)checked(size);
    const auto payloadBytes=compressed?size:source.size();
    if(!compressed)std::memcpy(out.data()+40,source.data(),source.size());
    put64(out,8,fingerprint);put64(out,16,source.size());put64(out,24,payloadBytes);put64(out,32,compressed?1:0);
    out.resize(40+payloadBytes);return out;
}
namespace {
template<class Validate>
std::optional<std::string> decode(std::string_view envelope,std::uint64_t fingerprint,Validate validate,bool retain){
    if(envelope.size()<24||read64(envelope,8)!=fingerprint)return std::nullopt;
    const auto plainBytes=read64(envelope,16);
    if(envelope.substr(0,8)=="AZCACHE1"){
        if(plainBytes!=envelope.size()-24)return std::nullopt;
        const auto source=envelope.substr(24);if(!validate(source))return std::nullopt;return retain?std::string(source):std::string{};
    }
    if(envelope.substr(0,8)!="AZCACHE2"||envelope.size()<40
        ||plainBytes<kAssetCacheCompressionMinimum||plainBytes>kAssetCacheCompressionMaximum
        ||read64(envelope,24)!=envelope.size()-40)return std::nullopt;
    const auto method=read64(envelope,32);const auto payload=envelope.substr(40);
    if(method==0){if(payload.size()!=plainBytes||!validate(payload))return std::nullopt;return retain?std::string(payload):std::string{};}
    if(method!=1||ZSTD_getFrameContentSize(payload.data(),payload.size())!=plainBytes)return std::nullopt;
    // Enforce the frame's declared window before allocating output. The public
    // decoder window parameter only limits internal buffers in streaming mode.
    if(payload.size()<5||(static_cast<unsigned char>(payload[4])&4U)==0)return std::nullopt;
    if((static_cast<unsigned char>(payload[4])&32U)==0){
        if(payload.size()<6)return std::nullopt;
        const auto descriptor=static_cast<unsigned char>(payload[5]);
        const auto base=std::uint64_t{1}<<(10+(descriptor>>3));
        if(base+(base/8)*(descriptor&7)>kAssetCacheCompressionMaximum)return std::nullopt;
    }
    const auto frameBytes=ZSTD_findFrameCompressedSize(payload.data(),payload.size());
    if(ZSTD_isError(frameBytes)||frameBytes!=payload.size())return std::nullopt;
    std::unique_ptr<ZSTD_DCtx,decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(),ZSTD_freeDCtx);
    if(!context)throw std::bad_alloc();
    if(ZSTD_isError(ZSTD_DCtx_setParameter(context.get(),ZSTD_d_windowLogMax,28)))return std::nullopt;
    std::string source(static_cast<std::size_t>(plainBytes),'\0');
    const auto size=ZSTD_decompressDCtx(context.get(),source.data(),source.size(),payload.data(),payload.size());
    if(ZSTD_isError(size)||size!=source.size()||!validate(source))return std::nullopt;
    return retain?std::move(source):std::string{};
}
}
std::optional<std::string> decodeAssetSourceCache(std::string_view envelope,std::uint64_t fingerprint,std::uint64_t sourceHash){
    return decode(envelope,fingerprint,[&](std::string_view source){return hash(source)==sourceHash;},true);
}
bool assetSourceCacheMatches(std::string_view envelope,std::uint64_t fingerprint,std::string_view source){
    return decode(envelope,fingerprint,[&](std::string_view cached){return cached==source;},false).has_value();
}
}
