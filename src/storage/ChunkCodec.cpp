#include "storage/ChunkCodec.hpp"

#include "checkpoint/CRC32C.hpp"
#include "checkpoint/GMPSerialization.hpp"
#include "storage/CompressionCodec.hpp"
#include "storage/StorageTiming.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::storage
{

namespace
{

constexpr std::array<std::uint8_t, 8> chunkMagic{
    'G', 'M', 'P', 'I', 'C', 'H', 'N', 'K'
};

constexpr std::size_t checksumOffset = 128;
constexpr std::size_t checksumSize = 4;


void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}


void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}


void appendU64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}


void storeU32(
    std::vector<std::uint8_t>& output,
    std::size_t offset,
    std::uint32_t value
)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        output[offset++] = static_cast<std::uint8_t>(value >> shift);
    }
}


std::size_t checkedSize(std::uint64_t value)
{
    if (value > maxChunkPayloadBytes
        || (sizeof(std::size_t) < sizeof(std::uint64_t)
            && value > std::numeric_limits<std::size_t>::max()))
    {
        throw std::length_error("Chunk payload length is too large");
    }

    return static_cast<std::size_t>(value);
}


std::size_t checkedAdd(std::size_t left, std::size_t right)
{
    if (right > std::numeric_limits<std::size_t>::max() - left)
    {
        throw std::overflow_error("Chunk payload size overflow");
    }
    return left + right;
}


std::uint32_t calculateChecksum(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < ChunkCodec::version1HeaderSize)
    {
        throw std::invalid_argument("Truncated chunk checksum input");
    }

    std::uint32_t checksum = pi::checkpoint::CRC32C::calculate(
        bytes.first(checksumOffset)
    );
    constexpr std::array<std::uint8_t, checksumSize> zeroChecksum{};
    checksum = pi::checkpoint::CRC32C::calculate(zeroChecksum, checksum);
    checksum = pi::checkpoint::CRC32C::calculate(
        bytes.subspan(checksumOffset + checksumSize),
        checksum
    );
    return checksum;
}


std::vector<std::uint8_t> canonicalPayload(
    const pi::checkpoint::SerializedGMPInteger& p,
    const pi::checkpoint::SerializedGMPInteger& q,
    const pi::checkpoint::SerializedGMPInteger& t
)
{
    std::size_t size = checkedAdd(p.magnitude.size(), q.magnitude.size());
    size = checkedAdd(size, t.magnitude.size());
    std::vector<std::uint8_t> payload;
    payload.reserve(size);
    payload.insert(payload.end(), p.magnitude.begin(), p.magnitude.end());
    payload.insert(payload.end(), q.magnitude.begin(), q.magnitude.end());
    payload.insert(payload.end(), t.magnitude.begin(), t.magnitude.end());
    return payload;
}


std::vector<std::uint8_t> makeHeader(
    const ChunkMetadata& metadata,
    const pi::checkpoint::SerializedGMPInteger& p,
    const pi::checkpoint::SerializedGMPInteger& q,
    const pi::checkpoint::SerializedGMPInteger& t
)
{
    std::vector<std::uint8_t> header;
    header.reserve(ChunkCodec::version1HeaderSize);
    header.insert(header.end(), chunkMagic.begin(), chunkMagic.end());
    appendU16(header, metadata.formatVersion);
    appendU16(header, ChunkCodec::version1HeaderSize);
    appendU32(header, 0); // flags
    appendU16(
        header,
        static_cast<std::uint16_t>(metadata.identity.computation.algorithm)
    );
    appendU16(header, metadata.identity.computation.arithmeticVersion);
    appendU16(header, metadata.identity.computation.formulaVersion);
    appendU16(header, 0); // reserved
    appendU64(header, metadata.identity.computation.requestedDigits);
    appendU32(header, metadata.identity.computation.guardDigits);
    appendU32(header, 0); // reserved
    appendU64(header, metadata.identity.computation.workingDigits);
    appendU64(header, metadata.identity.computation.termCount);
    appendU64(header, metadata.identity.location.start);
    appendU64(header, metadata.identity.location.end);
    appendU32(header, metadata.identity.location.treeLevel);
    appendU16(header, static_cast<std::uint16_t>(metadata.compression));
    appendU16(header, static_cast<std::uint16_t>(metadata.checksumAlgorithm));
    appendU16(header, checksumSize);
    appendU16(header, 0); // reserved
    appendU64(header, metadata.uncompressedSize);
    appendU64(header, metadata.storedSize);
    header.push_back(static_cast<std::uint8_t>(p.sign));
    header.push_back(static_cast<std::uint8_t>(q.sign));
    header.push_back(static_cast<std::uint8_t>(t.sign));
    header.push_back(0); // reserved
    appendU64(header, p.magnitude.size());
    appendU64(header, q.magnitude.size());
    appendU64(header, t.magnitude.size());
    appendU32(header, 0); // checksum value slot

    if (header.size() != ChunkCodec::version1HeaderSize)
    {
        throw std::logic_error("Chunk header size invariant failed");
    }
    return header;
}


class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint8_t readU8() { return take(1).front(); }

    std::uint16_t readU16()
    {
        const auto data = take(2);
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[0]) << 8) | data[1]
        );
    }

    std::uint32_t readU32()
    {
        const auto data = take(4);
        std::uint32_t value = 0;
        for (const std::uint8_t byte : data)
        {
            value = (value << 8) | byte;
        }
        return value;
    }

    std::uint64_t readU64()
    {
        const auto data = take(8);
        std::uint64_t value = 0;
        for (const std::uint8_t byte : data)
        {
            value = (value << 8) | byte;
        }
        return value;
    }

    std::span<const std::uint8_t> take(std::size_t count)
    {
        if (count > bytes_.size() - position_)
        {
            throw std::invalid_argument("Truncated runtime chunk");
        }
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }

    [[nodiscard]]
    std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

} // namespace


ChunkMetadata ChunkCodec::createMetadata(
    const ChunkIdentity& identity,
    const bigint::GMPInteger& p,
    const bigint::GMPInteger& q,
    const bigint::GMPInteger& t,
    CompressionAlgorithm compression
)
{
    const auto serializedP = checkpoint::GMPSerialization::encode(p);
    const auto serializedQ = checkpoint::GMPSerialization::encode(q);
    const auto serializedT = checkpoint::GMPSerialization::encode(t);
    const auto payload = canonicalPayload(serializedP, serializedQ, serializedT);
    const auto& codec = CompressionCodecs::forAlgorithm(compression);
    const auto stored = codec.compress(payload, maxChunkPayloadBytes);

    ChunkMetadata metadata = ChunkMetadata::create(
        identity,
        payload.size(),
        stored.size(),
        ChunkChecksumAlgorithm::crc32c,
        0,
        compression
    );
    const auto header = makeHeader(metadata, serializedP, serializedQ, serializedT);
    std::vector<std::uint8_t> bytes = header;
    bytes.insert(bytes.end(), stored.begin(), stored.end());
    metadata.checksumValue = calculateChecksum(bytes);
    return metadata;
}


std::vector<std::uint8_t> ChunkCodec::encode(
    const Chunk& chunk,
    StorageTiming* timing
)
{
    chunk.metadata.validate();
    const auto encodeStarted = std::chrono::steady_clock::now();
    const auto serializedP = checkpoint::GMPSerialization::encode(chunk.p);
    const auto serializedQ = checkpoint::GMPSerialization::encode(chunk.q);
    const auto serializedT = checkpoint::GMPSerialization::encode(chunk.t);
    if (timing != nullptr)
        StorageTiming::add(
            timing->storeGmpEncodeNs,
            std::chrono::steady_clock::now() - encodeStarted);
    const std::size_t payloadSize = serializedP.magnitude.size()
        + serializedQ.magnitude.size() + serializedT.magnitude.size();
    if (chunk.metadata.uncompressedSize != payloadSize)
    {
        throw std::invalid_argument("Chunk uncompressed size metadata does not match payload");
    }

    std::vector<std::uint8_t> output = makeHeader(
        chunk.metadata,
        serializedP,
        serializedQ,
        serializedT
    );
    output.resize(output.size() + static_cast<std::size_t>(chunk.metadata.storedSize));
    const auto payloadOutput = std::span<std::uint8_t>(output).subspan(
        checksumOffset + checksumSize);
    const auto compressionStarted = std::chrono::steady_clock::now();
    std::size_t written = 0;
    if (chunk.metadata.compression == CompressionAlgorithm::none)
    {
        std::copy(serializedP.magnitude.begin(), serializedP.magnitude.end(), payloadOutput.begin());
        std::copy(serializedQ.magnitude.begin(), serializedQ.magnitude.end(),
            payloadOutput.begin() + serializedP.magnitude.size());
        std::copy(serializedT.magnitude.begin(), serializedT.magnitude.end(),
            payloadOutput.begin() + serializedP.magnitude.size() + serializedQ.magnitude.size());
        written = payloadSize;
    }
    else
    {
        const auto payload = canonicalPayload(serializedP, serializedQ, serializedT);
        const auto& codec = CompressionCodecs::forAlgorithm(
            chunk.metadata.compression
        );
        written = codec.compressInto(payload, payloadOutput);
    }
    if (timing != nullptr)
        StorageTiming::add(
            timing->storeCompressionNs,
            std::chrono::steady_clock::now() - compressionStarted);
    if (written != chunk.metadata.storedSize)
        throw std::invalid_argument("Chunk stored size metadata does not match payload");
    const std::uint32_t checksum = calculateChecksum(output);
    if (chunk.metadata.checksumValue != checksum)
    {
        throw std::invalid_argument("Chunk checksum metadata does not match payload");
    }
    storeU32(output, checksumOffset, checksum);
    return output;
}


Chunk ChunkCodec::decode(
    std::span<const std::uint8_t> bytes,
    StorageTiming* timing
)
{
    Reader reader(bytes);
    const auto magic = reader.take(chunkMagic.size());
    if (!std::equal(magic.begin(), magic.end(), chunkMagic.begin()))
    {
        throw std::invalid_argument("Invalid runtime chunk magic");
    }

    if (reader.readU16() != formatVersion
        || reader.readU16() != version1HeaderSize)
    {
        throw std::invalid_argument("Unsupported runtime chunk version");
    }
    if (reader.readU32() != 0)
    {
        throw std::invalid_argument("Unsupported runtime chunk flags");
    }

    const checkpoint::AlgorithmId algorithm =
        static_cast<checkpoint::AlgorithmId>(reader.readU16());
    const std::uint16_t arithmeticVersion = reader.readU16();
    const std::uint16_t formulaVersion = reader.readU16();
    if (reader.readU16() != 0)
    {
        throw std::invalid_argument("Non-zero runtime chunk reserved field");
    }

    checkpoint::ComputationIdentity computation{
        algorithm,
        arithmeticVersion,
        formulaVersion,
        reader.readU64(),
        reader.readU32(),
        0,
        0
    };
    if (reader.readU32() != 0)
    {
        throw std::invalid_argument("Non-zero runtime chunk reserved field");
    }
    computation.workingDigits = reader.readU64();
    computation.termCount = reader.readU64();
    computation.validate();

    checkpoint::BlockLocation location{
        reader.readU64(), reader.readU64(), reader.readU32()
    };
    location.validate(computation);

    const auto compression =
        static_cast<CompressionAlgorithm>(reader.readU16());
    const auto checksumAlgorithm =
        static_cast<ChunkChecksumAlgorithm>(reader.readU16());
    const std::uint16_t checksumLength = reader.readU16();
    if (checksumLength != checksumSize || reader.readU16() != 0)
    {
        throw std::invalid_argument("Invalid runtime chunk checksum metadata");
    }

    const std::uint64_t uncompressedSize = reader.readU64();
    const std::uint64_t storedSize = reader.readU64();
    const auto pSign = static_cast<checkpoint::IntegerSign>(reader.readU8());
    const auto qSign = static_cast<checkpoint::IntegerSign>(reader.readU8());
    const auto tSign = static_cast<checkpoint::IntegerSign>(reader.readU8());
    if (reader.readU8() != 0)
    {
        throw std::invalid_argument("Non-zero runtime chunk reserved field");
    }

    const std::size_t pLength = checkedSize(reader.readU64());
    const std::size_t qLength = checkedSize(reader.readU64());
    const std::size_t tLength = checkedSize(reader.readU64());
    const std::uint32_t checksumValue = reader.readU32();

    const ChunkIdentity identity = ChunkIdentity::create(computation, location);
    const ChunkMetadata metadata = ChunkMetadata::create(
        identity,
        uncompressedSize,
        storedSize,
        checksumAlgorithm,
        checksumValue,
        compression
    );

    const auto crcStarted = std::chrono::steady_clock::now();
    const auto calculatedChecksum = calculateChecksum(bytes);
    if (timing != nullptr)
        StorageTiming::add(
            timing->loadCrcNs,
            std::chrono::steady_clock::now() - crcStarted);
    if (storedSize != reader.remaining() || checksumValue != calculatedChecksum)
    {
        throw std::invalid_argument("Runtime chunk checksum or size mismatch");
    }

    const auto stored = reader.take(reader.remaining());
    const auto& codec = CompressionCodecs::forAlgorithm(compression);
    const auto decompressionStarted = std::chrono::steady_clock::now();
    const auto payload = codec.decompress(
        stored,
        uncompressedSize,
        maxChunkPayloadBytes
    );
    if (timing != nullptr)
        StorageTiming::add(
            timing->loadDecompressionNs,
            std::chrono::steady_clock::now() - decompressionStarted);
    const std::size_t payloadLength = checkedAdd(
        checkedAdd(pLength, qLength),
        tLength
    );
    if (payloadLength != payload.size())
    {
        throw std::invalid_argument("Runtime chunk payload lengths mismatch");
    }

    const auto pBytes = std::span<const std::uint8_t>(payload).first(pLength);
    const auto qBytes = std::span<const std::uint8_t>(payload).subspan(
        pLength,
        qLength
    );
    const auto tBytes = std::span<const std::uint8_t>(payload).subspan(
        pLength + qLength,
        tLength
    );

    const auto decodeStarted = std::chrono::steady_clock::now();
    Chunk result{
        metadata,
        checkpoint::GMPSerialization::decode(pSign, pBytes),
        checkpoint::GMPSerialization::decode(qSign, qBytes),
        checkpoint::GMPSerialization::decode(tSign, tBytes)
    };
    if (timing != nullptr)
        StorageTiming::add(
            timing->loadGmpDecodeNs,
            std::chrono::steady_clock::now() - decodeStarted);
    return result;
}

} // namespace pi::storage
