#pragma once

#include "bigint/GMPInteger.hpp"
#include "storage/ChunkTypes.hpp"

#include <cstdint>
#include <span>
#include <vector>


namespace pi::storage
{

struct StorageTiming;

struct Chunk
{
    ChunkMetadata metadata;
    bigint::GMPInteger p;
    bigint::GMPInteger q;
    bigint::GMPInteger t;
};


class ChunkCodec
{
public:
    static constexpr std::uint16_t formatVersion = chunkFormatVersion;
    static constexpr std::uint16_t version1HeaderSize = 132;

    [[nodiscard]]
    static ChunkMetadata createMetadata(
        const ChunkIdentity& identity,
        const bigint::GMPInteger& p,
        const bigint::GMPInteger& q,
        const bigint::GMPInteger& t,
        CompressionAlgorithm compression = CompressionAlgorithm::none
    );

    /// Encodes a chunk and optionally accumulates stage timings.
    [[nodiscard]]
    static std::vector<std::uint8_t> encode(
        const Chunk& chunk,
        StorageTiming* timing = nullptr
    );

    /// Decodes a chunk and optionally accumulates stage timings.
    [[nodiscard]]
    static Chunk decode(
        std::span<const std::uint8_t> bytes,
        StorageTiming* timing = nullptr
    );
};

} // namespace pi::storage
