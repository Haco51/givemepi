#include "storage/CompressionCodec.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>


int main()
{
    using pi::storage::CompressionAlgorithm;
    using pi::storage::CompressionCodecs;

    const auto& codec = CompressionCodecs::forAlgorithm(
        CompressionAlgorithm::none
    );
    const std::vector<std::uint8_t> input{1, 2, 3, 4};
    const std::vector<std::uint8_t> compressed = codec.compress(input, 4);
    const std::vector<std::uint8_t> decompressed = codec.decompress(
        compressed,
        4,
        4
    );

    if (compressed != input || decompressed != input
        || codec.algorithm() != CompressionAlgorithm::none)
    {
        std::cerr << "None compression round trip mismatch\n";
        return 1;
    }

    bool rejected = false;
    try
    {
        static_cast<void>(codec.compress(input, 3));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    if (!rejected)
    {
        std::cerr << "Compression allocation bound was ignored\n";
        return 1;
    }

    rejected = false;
    try
    {
        static_cast<void>(codec.decompress(input, 3, 4));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    if (!rejected)
    {
        std::cerr << "Decompression size contract was ignored\n";
        return 1;
    }

    const auto& lz4 = CompressionCodecs::forAlgorithm(
        CompressionAlgorithm::lz4);
    const std::vector<std::uint8_t> repetitive(4096, 0x2a);
    const auto lz4Compressed = lz4.compress(repetitive, 4096);
    const auto lz4Decompressed = lz4.decompress(
        lz4Compressed, repetitive.size(), repetitive.size());
    if (lz4.algorithm() != CompressionAlgorithm::lz4
        || lz4Decompressed != repetitive
        || lz4Compressed.size() >= repetitive.size())
    {
        std::cerr << "LZ4 compression round trip mismatch\n";
        return 1;
    }

    std::cout << "CompressionCodec OK\n";
    return 0;
}
