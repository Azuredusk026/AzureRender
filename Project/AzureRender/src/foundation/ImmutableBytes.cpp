#include "foundation/ImmutableBytes.hpp"
#include <zstd.h>
#include <zstd_errors.h>
#include <cstring>
#include <stdexcept>
#include <new>

namespace azurerender {
std::uint32_t immutableBytesBucket(const std::vector<std::uint8_t>& bytes) noexcept {
    std::uint32_t hash=2166136261U;std::size_t offset=0;
    for(;bytes.size()-offset>=sizeof(std::uint32_t);offset+=sizeof(std::uint32_t)) {
        std::uint32_t word;std::memcpy(&word,bytes.data()+offset,sizeof(word));
        hash=(hash^word)*16777619U;
    }
    for(;offset<bytes.size();++offset)hash=(hash^bytes[offset])*16777619U;
    return hash;
}
namespace {
void checked(std::size_t code) {
    if(ZSTD_isError(code))throw std::runtime_error(std::string("Immutable byte compression: ")+ZSTD_getErrorName(code));
}

template<class Match>
std::shared_ptr<const ImmutableBytes> findPayload(
    std::unordered_multimap<std::uint32_t, std::weak_ptr<const ImmutableBytes>>& entries,
    std::uint32_t bucket, Match match) {
    const auto range = entries.equal_range(bucket);
    for (auto at = range.first; at != range.second;) {
        const auto existing = at->second.lock();
        if (!existing) {
            at = entries.erase(at);
            continue;
        }
        if (match(*existing)) return existing;
        ++at;
    }
    return {};
}
}
ImmutableBytes::ImmutableBytes(const std::vector<std::uint8_t>& source,std::uint32_t bucket)
    :decodedBytes_(source.size()),bucket_(bucket) {
    constexpr std::size_t minimum=64*1024,maximum=256*1024*1024;
    if(source.size()>=minimum&&source.size()<=maximum) {
        std::unique_ptr<ZSTD_CCtx,decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(),ZSTD_freeCCtx);
        if(!context)throw std::bad_alloc();
        checked(ZSTD_CCtx_setParameter(context.get(),ZSTD_c_compressionLevel,1));
        checked(ZSTD_CCtx_setParameter(context.get(),ZSTD_c_checksumFlag,1));
        std::vector<std::uint8_t> encoded(source.size()*7/8);
        const auto size=ZSTD_compress2(context.get(),encoded.data(),encoded.size(),source.data(),source.size());
        if(!ZSTD_isError(size)) {
            // Own a right-sized allocation, rather than retaining the bound.
            storage_=std::vector<std::uint8_t>(encoded.begin(),encoded.begin()+size);
            compressed_=true;return;
        }
        if(ZSTD_getErrorCode(size)!=ZSTD_error_dstSize_tooSmall)checked(size);
    }
    storage_=source;
}
std::vector<std::uint8_t> ImmutableBytes::read() const {
    if(!compressed_)return storage_;
    std::vector<std::uint8_t> decoded(decodedBytes_);
    const auto size=ZSTD_decompress(decoded.data(),decoded.size(),storage_.data(),storage_.size());
    checked(size);
    if(size!=decodedBytes_)throw std::runtime_error("Immutable byte payload length mismatch");
    return decoded;
}
bool ImmutableBytes::equals(const std::vector<std::uint8_t>& other) const {
    if(other.size()!=decodedBytes_)return false;
    return compressed_?read()==other:storage_==other;
}
bool ImmutableBytes::equals(const ImmutableBytes& other) const {
    if(this==&other)return true;
    if(decodedBytes_!=other.decodedBytes_)return false;
    if(compressed_==other.compressed_&&storage_==other.storage_)return true;
    return read()==other.read();
}
std::shared_ptr<const ImmutableBytes> ImmutableBytePool::intern(const std::vector<std::uint8_t>& bytes) {
    const auto bucket = immutableBytesBucket(bytes);
    if (auto existing = findPayload(entries_, bucket,
            [&](const ImmutableBytes& value) { return value.equals(bytes); }))
        return existing;

    auto payload = std::shared_ptr<const ImmutableBytes>(new ImmutableBytes(bytes, bucket));
    entries_.emplace(bucket, payload);
    return payload;
}
std::shared_ptr<const ImmutableBytes> ImmutableBytePool::adopt(std::shared_ptr<const ImmutableBytes> candidate) {
    if(!candidate)throw std::invalid_argument("An immutable byte payload is required");
    const auto bucket=candidate->bucket();
    if (auto existing = findPayload(entries_, bucket,
            [&](const ImmutableBytes& value) { return value.equals(*candidate); }))
        return existing;
    entries_.emplace(bucket,candidate);return candidate;
}
ImmutableBytePoolStatistics ImmutableBytePool::statistics() const noexcept {
    ImmutableBytePoolStatistics result;
    for(const auto& entry:entries_)if(const auto value=entry.second.lock()) {
        ++result.livePayloads;result.decodedBytes+=value->size();result.storedBytes+=value->storedBytes();
    }
    return result;
}
}
