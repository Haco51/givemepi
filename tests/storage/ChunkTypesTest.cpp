#include "storage/ChunkTypes.hpp"

#include <iostream>
#include <stdexcept>


using pi::checkpoint::BlockLocation;
using pi::checkpoint::ComputationIdentity;
using pi::chudnovsky::PrecisionPolicy;
using pi::storage::ChunkChecksumAlgorithm;
using pi::storage::ChunkIdentity;
using pi::storage::ChunkMetadata;
using pi::storage::CompressionAlgorithm;


namespace
{

template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }

    return false;
}

} // namespace


int main()
{
    const ComputationIdentity computation =
        ComputationIdentity::fromPrecisionPlan(
            PrecisionPolicy::create(1000, 32)
        );
    const BlockLocation location = BlockLocation::create(
        10,
        20,
        3,
        computation
    );
    const ChunkIdentity identity = ChunkIdentity::create(
        computation,
        location
    );

    const std::string expectedName =
        "chunk-v1-a1-av1-fv1-d1000-g32-w1032-n74-r10-20-l3.chunk";
    if (identity.deterministicFilename() != expectedName)
    {
        std::cerr << "Deterministic chunk filename mismatch\n";
        return 1;
    }

    const ChunkMetadata metadata = ChunkMetadata::create(
        identity,
        4096,
        4096,
        ChunkChecksumAlgorithm::crc32c,
        0x12345678U
    );
    if (metadata.identity != identity
        || metadata.uncompressedSize != 4096
        || metadata.storedSize != 4096
        || metadata.checksumValue != 0x12345678U
        || metadata.compression != CompressionAlgorithm::none
        || metadata.formatVersion != 1)
    {
        std::cerr << "Chunk metadata mismatch\n";
        return 1;
    }

    if (!throws<std::invalid_argument>([&identity]() {
            static_cast<void>(ChunkMetadata::create(
                identity,
                pi::storage::maxChunkPayloadBytes + 1,
                1,
                ChunkChecksumAlgorithm::crc32c,
                0
            ));
        })
        || !throws<std::invalid_argument>([&identity]() {
            static_cast<void>(ChunkMetadata::create(
                identity,
                1,
                1,
                ChunkChecksumAlgorithm::crc32c,
                0,
                CompressionAlgorithm::none,
                2
            ));
        }))
    {
        std::cerr << "Invalid chunk metadata was accepted\n";
        return 1;
    }

    if (identity != ChunkIdentity::create(computation, location))
    {
        std::cerr << "Chunk identity is not deterministic\n";
        return 1;
    }

    std::cout << "ChunkTypes OK\n";
    return 0;
}
