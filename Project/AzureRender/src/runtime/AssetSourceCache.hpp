#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace azurerender {
// Pure, versioned source snapshots. The database owns identity verification,
// atomic publication and source lifetimes; this codec owns no files or threads.
inline constexpr std::size_t kAssetCacheCompressionMinimum=64*1024;
inline constexpr std::size_t kAssetCacheCompressionMaximum=256*1024*1024;
std::string encodeAssetSourceCache(std::string_view source,std::uint64_t fingerprint);
// Invalid, truncated, unsupported or identity-mismatched envelopes are misses.
std::optional<std::string> decodeAssetSourceCache(std::string_view envelope,
    std::uint64_t fingerprint,std::uint64_t sourceHash);
// Warm publication already owns verified source bytes. Compare them directly
// after envelope validation, without hashing or copying raw payloads again.
bool assetSourceCacheMatches(std::string_view envelope,std::uint64_t fingerprint,std::string_view source);
}
