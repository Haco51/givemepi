#pragma once

#include "storage/ChunkTypes.hpp"

#include <cstdint>
#include <span>
#include <vector>


namespace pi::storage
{

class CompressionCodec
{
public:
    virtual ~CompressionCodec() = default;

    [[nodiscard]]
    virtual CompressionAlgorithm algorithm() const noexcept = 0;

    [[nodiscard]]
    virtual std::vector<std::uint8_t> compress(
        std::span<const std::uint8_t> input,
        std::uint64_t maxOutputSize
    ) const = 0;

    [[nodiscard]]
    virtual std::size_t compressInto(
        std::span<const std::uint8_t> input,
        std::span<std::uint8_t> output
    ) const = 0;

    [[nodiscard]]
    virtual std::vector<std::uint8_t> decompress(
        std::span<const std::uint8_t> input,
        std::uint64_t expectedOutputSize,
        std::uint64_t maxOutputSize
    ) const = 0;
};


class CompressionCodecs
{
public:
    CompressionCodecs() = delete;

    [[nodiscard]]
    static const CompressionCodec& forAlgorithm(
        CompressionAlgorithm algorithm
    );
};

} // namespace pi::storage
