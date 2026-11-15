#pragma once

#include <spirv/unified1/spirv.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace azurerender {

// Give a single fragment entry point the strict 32-bit arithmetic policy.
// Restore Vulkan's ordinary finite-value and reciprocal policy. Preserve the
// expression's rounding steps by excluding contraction and reassociation.
inline std::vector<char> prepareStrictFragmentShader(const std::vector<char>& input) {
    constexpr std::uint32_t version12 = 0x00010200;
    constexpr std::uint32_t version16 = 0x00010600;
    constexpr std::size_t headerWords = 5;
    constexpr std::uint32_t arithmeticMask = SpvFPFastMathModeNotNaNMask | SpvFPFastMathModeNotInfMask
        | SpvFPFastMathModeNSZMask | SpvFPFastMathModeAllowRecipMask;
    const auto reject = [] { throw std::invalid_argument("Invalid SPIR-V for strict fragment arithmetic"); };
    if (input.size() < headerWords * sizeof(std::uint32_t) || input.size() % sizeof(std::uint32_t))
        reject();
    std::vector<std::uint32_t> words(input.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), input.data(), input.size());
    if (words[0] != SpvMagicNumber || words[1] < 0x00010000 || words[1] > version16
        || !words[3] || words[4])
        reject();

    std::size_t capabilityEnd = headerWords;
    std::size_t modeEnd = 0;
    std::size_t unsignedTypeEnd = 0;
    std::uint32_t entry = 0, floatType = 0, unsignedType = 0, maskConstant = 0;
    const auto validId = [&](std::uint32_t id) { return id && id < words[3]; };
    for (std::size_t at = headerWords; at < words.size();) {
        const auto count = words[at] >> 16;
        const auto opcode = words[at] & 0xffff;
        if (!count || count > words.size() - at) reject();
        const auto next = at + count;
        switch (opcode) {
        case SpvOpCapability:
            if (count != 2 || at != capabilityEnd || words[at + 1] == SpvCapabilityFloatControls2)
                reject();
            capabilityEnd = next;
            break;
        case SpvOpEntryPoint:
            if (count < 4 || entry || words[at + 1] != SpvExecutionModelFragment || !validId(words[at + 2]))
                reject();
            entry = words[at + 2];
            modeEnd = next;
            break;
        case SpvOpExecutionMode:
        case SpvOpExecutionModeId:
            if (count < 3 || !entry || words[at + 1] != entry
                || words[at + 2] == SpvExecutionModeFPFastMathDefault)
                reject();
            modeEnd = next;
            break;
        case SpvOpTypeFloat:
            if (count != 3 || !validId(words[at + 1])) reject();
            if (words[at + 2] == 32) floatType = words[at + 1];
            break;
        case SpvOpTypeInt:
            if (count != 4 || !validId(words[at + 1])) reject();
            if (words[at + 2] == 32 && words[at + 3] == 0) {
                unsignedType = words[at + 1];
                unsignedTypeEnd = next;
            }
            break;
        case SpvOpConstant:
            if (count < 4) reject();
            if (count == 4 && unsignedType && words[at + 1] == unsignedType && words[at + 3] == arithmeticMask) {
                if (!validId(words[at + 2])) reject();
                maskConstant = words[at + 2];
            }
            break;
        case SpvOpDecorate:
            if (count < 3 || words[at + 2] == SpvDecorationFPFastMathMode
                || (words[at + 2] == SpvDecorationNoContraction && count != 3))
                reject();
            break;
        default:
            break;
        }
        at = next;
    }
    if (capabilityEnd == headerWords || !entry || !modeEnd || !floatType || !unsignedType)
        reject();

    const bool addMask = !maskConstant;
    if (addMask) {
        if (words[3] >= 0x3fffffU) reject();
        maskConstant = words[3]++;
    }
    words[1] = std::max(words[1], version12);
    std::vector<std::uint32_t> result(words.begin(), words.begin() + headerWords);
    result.reserve(words.size() + 18);
    constexpr char extension[] = "SPV_KHR_float_controls2";
    constexpr std::uint32_t extensionWords = (sizeof(extension) + 3U) / 4U;

    for (std::size_t at = headerWords; at <= words.size();) {
        if (at == capabilityEnd) {
            result.insert(result.end(), {(2U << 16) | SpvOpCapability, SpvCapabilityFloatControls2,
                                         ((extensionWords + 1U) << 16) | SpvOpExtension});
            const auto start = result.size();
            result.resize(start + extensionWords);
            std::memcpy(result.data() + start, extension, sizeof(extension));
        }
        if (at == modeEnd)
            result.insert(result.end(), {(5U << 16) | SpvOpExecutionModeId, entry,
                                         SpvExecutionModeFPFastMathDefault, floatType, maskConstant});
        if (addMask && at == unsignedTypeEnd)
            result.insert(result.end(), {(4U << 16) | SpvOpConstant, unsignedType, maskConstant, arithmeticMask});
        if (at == words.size()) break;
        const auto count = words[at] >> 16;
        const auto opcode = words[at] & 0xffff;
        if (opcode != SpvOpDecorate || words[at + 2] != SpvDecorationNoContraction)
            result.insert(result.end(), words.data() + at, words.data() + at + count);
        at += count;
    }
    std::vector<char> output(result.size() * sizeof(std::uint32_t));
    std::memcpy(output.data(), result.data(), output.size());
    return output;
}

} // namespace azurerender
