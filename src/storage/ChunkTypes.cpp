#include "storage/ChunkTypes.hpp"

#include <stdexcept>
#include <utility>


namespace pi::storage
{

ChunkIdentity ChunkIdentity::create(
    const checkpoint::ComputationIdentity& computation,
    const checkpoint::BlockLocation& location
)
{
    ChunkIdentity identity{computation, location};
    identity.validate();
    return identity;
}


void ChunkIdentity::validate() const
{
    computation.validate();
    location.validate(computation);
}


std::string ChunkIdentity::deterministicFilename() const
{
    validate();

    return "chunk-v" + std::to_string(chunkFormatVersion)
        + "-a"
        + std::to_string(static_cast<std::uint16_t>(computation.algorithm))
        + "-av" + std::to_string(computation.arithmeticVersion)
        + "-fv" + std::to_string(computation.formulaVersion)
        + "-d" + std::to_string(computation.requestedDigits)
        + "-g" + std::to_string(computation.guardDigits)
        + "-w" + std::to_string(computation.workingDigits)
        + "-n" + std::to_string(computation.termCount)
        + "-r" + std::to_string(location.start)
        + "-" + std::to_string(location.end)
        + "-l" + std::to_string(location.treeLevel)
        + ".chunk";
}


ChunkMetadata ChunkMetadata::create(
    ChunkIdentity identity,
    std::uint64_t uncompressedSize,
    std::uint64_t storedSize,
    ChunkChecksumAlgorithm checksumAlgorithm,
    std::uint32_t checksumValue,
    CompressionAlgorithm compression,
    std::uint16_t formatVersion
)
{
    ChunkMetadata metadata{
        std::move(identity),
        uncompressedSize,
        storedSize,
        checksumAlgorithm,
        checksumValue,
        compression,
        formatVersion
    };
    metadata.validate();
    return metadata;
}


void ChunkMetadata::validate() const
{
    identity.validate();

    if (formatVersion != chunkFormatVersion)
    {
        throw std::invalid_argument("Unsupported runtime chunk format version");
    }

    if (uncompressedSize > maxChunkPayloadBytes
        || storedSize > maxChunkPayloadBytes)
    {
        throw std::invalid_argument("Runtime chunk size exceeds the limit");
    }

    if (checksumAlgorithm != ChunkChecksumAlgorithm::crc32c)
    {
        throw std::invalid_argument(
            "Unsupported runtime chunk checksum algorithm"
        );
    }

    if (compression != CompressionAlgorithm::none
        && compression != CompressionAlgorithm::lz4)
    {
        throw std::invalid_argument(
            "Runtime chunk compression is not implemented"
        );
    }
}

} // namespace pi::storage

// ============================================================================
// ChunkMetadata::createChunkFilename 구현
// ============================================================================

namespace pi::storage
{

std::string ChunkMetadata::createChunkFilename(ChunkId chunkId)
{
    // chunkId 가 "/" 를 포함하지 않도록 확인 (filename 으로 사용 가능)
    if (chunkId.find('/') != std::string::npos)
    {
        throw std::invalid_argument("ChunkId cannot contain '/': " + chunkId);
    }
    
    return chunkId;
}

} // namespace pi::storage
