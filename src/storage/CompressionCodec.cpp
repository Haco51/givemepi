#include "storage/CompressionCodec.hpp"

#include <lz4.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>


namespace pi::storage
{

namespace
{

std::size_t checkedSize(std::uint64_t value, std::uint64_t limit)
{
    if (value > limit
        || (sizeof(std::size_t) < sizeof(std::uint64_t)
            && value > std::numeric_limits<std::size_t>::max()))
    {
        throw std::length_error("Compression output size is too large");
    }

    return static_cast<std::size_t>(value);
}

int checkedLz4Size(std::uint64_t value)
{
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("LZ4 input exceeds the API limit");
    }
    return static_cast<int>(value);
}


class NoneCompressionCodec final : public CompressionCodec
{
public:
    [[nodiscard]]
    CompressionAlgorithm algorithm() const noexcept override
    {
        return CompressionAlgorithm::none;
    }

    [[nodiscard]]
    std::vector<std::uint8_t> compress(
        std::span<const std::uint8_t> input,
        std::uint64_t maxOutputSize
    ) const override
    {
        static_cast<void>(checkedSize(
            input.size(),
            maxOutputSize
        ));
        return std::vector<std::uint8_t>(input.begin(), input.end());
    }

    [[nodiscard]]
    std::vector<std::uint8_t> decompress(
        std::span<const std::uint8_t> input,
        std::uint64_t expectedOutputSize,
        std::uint64_t maxOutputSize
    ) const override
    {
        const std::size_t outputSize = checkedSize(
            expectedOutputSize,
            maxOutputSize
        );
        if (input.size() != outputSize)
        {
            throw std::invalid_argument(
                "Uncompressed payload size does not match metadata"
            );
        }

        return std::vector<std::uint8_t>(input.begin(), input.end());
    }
};

class Lz4CompressionCodec final : public CompressionCodec
{
public:
    [[nodiscard]]
    CompressionAlgorithm algorithm() const noexcept override
    {
        return CompressionAlgorithm::lz4;
    }

    [[nodiscard]]
    std::vector<std::uint8_t> compress(
        std::span<const std::uint8_t> input,
        std::uint64_t maxOutputSize
    ) const override
    {
        const int inputSize = checkedLz4Size(input.size());
        const int outputCapacity = checkedLz4Size(
            std::min<std::uint64_t>(
                maxOutputSize,
                static_cast<std::uint64_t>(LZ4_compressBound(inputSize))));
        std::vector<std::uint8_t> output(
            static_cast<std::size_t>(outputCapacity));
        const int written = LZ4_compress_default(
            reinterpret_cast<const char*>(input.data()),
            reinterpret_cast<char*>(output.data()),
            inputSize,
            outputCapacity);
        if (written <= 0)
        {
            throw std::length_error("LZ4 compression output exceeds the limit");
        }
        output.resize(static_cast<std::size_t>(written));
        return output;
    }

    [[nodiscard]]
    std::vector<std::uint8_t> decompress(
        std::span<const std::uint8_t> input,
        std::uint64_t expectedOutputSize,
        std::uint64_t maxOutputSize
    ) const override
    {
        const std::size_t outputSize = checkedSize(
            expectedOutputSize,
            maxOutputSize);
        const int inputSize = checkedLz4Size(input.size());
        const int outputCapacity = checkedLz4Size(outputSize);
        std::vector<std::uint8_t> output(outputSize);
        const int decoded = LZ4_decompress_safe(
            reinterpret_cast<const char*>(input.data()),
            reinterpret_cast<char*>(output.data()),
            inputSize,
            outputCapacity);
        if (decoded != outputCapacity)
        {
            throw std::invalid_argument("LZ4 decompression size mismatch");
        }
        return output;
    }
};

} // namespace


const CompressionCodec& CompressionCodecs::forAlgorithm(
    CompressionAlgorithm algorithm
)
{
    static const NoneCompressionCodec noneCodec;
    static const Lz4CompressionCodec lz4Codec;

    if (algorithm == CompressionAlgorithm::none)
    {
        return noneCodec;
    }
    if (algorithm == CompressionAlgorithm::lz4)
    {
        return lz4Codec;
    }

    throw std::invalid_argument(
        "Compression codec is not implemented: "
        + std::string(
            algorithm == CompressionAlgorithm::lz4 ? "lz4" : "unknown"
        )
    );
}

} // namespace pi::storage
