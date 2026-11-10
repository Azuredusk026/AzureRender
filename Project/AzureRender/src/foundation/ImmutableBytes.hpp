#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace azurerender {
// A process-local bucket. Every byte participates; equality remains exact.
std::uint32_t immutableBytesBucket(const std::vector<std::uint8_t>& bytes) noexcept;

class ImmutableBytePool;
class ImmutableBytes final {
public:
    ImmutableBytes(const ImmutableBytes&)=delete;
    ImmutableBytes& operator=(const ImmutableBytes&)=delete;
    std::size_t size() const noexcept {return decodedBytes_;}
    std::size_t storedBytes() const noexcept {return storage_.capacity();}
    std::uint32_t bucket() const noexcept {return bucket_;}
    bool compressed() const noexcept {return compressed_;}
    std::vector<std::uint8_t> read() const;
    bool equals(const std::vector<std::uint8_t>&) const;
    bool equals(const ImmutableBytes&) const;
private:
    friend class ImmutableBytePool;
    ImmutableBytes(const std::vector<std::uint8_t>&,std::uint32_t);
    std::vector<std::uint8_t> storage_;
    std::size_t decodedBytes_=0;
    std::uint32_t bucket_=0;
    bool compressed_=false;
};

struct ImmutableBytePoolStatistics {
    std::uint64_t livePayloads=0,decodedBytes=0,storedBytes=0;
};

// The owner serializes pool calls. Payloads are immutable and may be read by
// independent threads; each decompression owns its context and output.
class ImmutableBytePool final {
public:
    std::shared_ptr<const ImmutableBytes> intern(const std::vector<std::uint8_t>&);
    ImmutableBytePoolStatistics statistics() const noexcept;
private:
    std::unordered_multimap<std::uint32_t,std::weak_ptr<const ImmutableBytes>> entries_;
};
}
