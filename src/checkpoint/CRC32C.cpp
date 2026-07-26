#include "checkpoint/CRC32C.hpp"

#include <array>
#include <cstddef>
#include <cstring>

#if (defined(__x86_64__) || defined(__i386__)) \
    && (defined(__GNUC__) || defined(__clang__))
#include <nmmintrin.h>
#define PI_CRC32C_HAS_SSE42 1
#endif


namespace pi::checkpoint
{

namespace
{

constexpr std::uint32_t reflectedCastagnoliPolynomial = 0x82F63B78U;


constexpr std::array<std::uint32_t, 256> createTable() noexcept
{
    std::array<std::uint32_t, 256> table{};

    for (std::size_t index = 0; index < table.size(); ++index)
    {
        std::uint32_t remainder = static_cast<std::uint32_t>(index);

        for (int bit = 0; bit < 8; ++bit)
        {
            remainder = (remainder >> 1)
                ^ ((remainder & 1U) != 0
                    ? reflectedCastagnoliPolynomial
                    : 0U);
        }

        table[index] = remainder;
    }

    return table;
}


constexpr auto crcTable = createTable();

#if defined(PI_CRC32C_HAS_SSE42)
__attribute__((target("sse4.2")))
std::uint32_t calculateSse42(
    std::span<const std::uint8_t> bytes,
    std::uint32_t previous
) noexcept
{
    std::uint32_t checksum = previous ^ 0xFFFFFFFFU;
    std::size_t offset = 0;

#if defined(__x86_64__)
    while (bytes.size() - offset >= sizeof(std::uint64_t))
    {
        std::uint64_t word = 0;
        std::memcpy(&word, bytes.data() + offset, sizeof(word));
        checksum = static_cast<std::uint32_t>(
            _mm_crc32_u64(checksum, word));
        offset += sizeof(word);
    }
#endif
    while (bytes.size() - offset >= sizeof(std::uint32_t))
    {
        std::uint32_t word = 0;
        std::memcpy(&word, bytes.data() + offset, sizeof(word));
        checksum = _mm_crc32_u32(checksum, word);
        offset += sizeof(word);
    }
    while (bytes.size() - offset >= sizeof(std::uint16_t))
    {
        std::uint16_t word = 0;
        std::memcpy(&word, bytes.data() + offset, sizeof(word));
        checksum = _mm_crc32_u16(checksum, word);
        offset += sizeof(word);
    }
    while (offset < bytes.size())
        checksum = _mm_crc32_u8(checksum, bytes[offset++]);

    return checksum ^ 0xFFFFFFFFU;
}

bool hasSse42() noexcept
{
    static const bool available = __builtin_cpu_supports("sse4.2") != 0;
    return available;
}
#endif

} // namespace


std::uint32_t CRC32C::calculate(
    std::span<const std::uint8_t> bytes,
    std::uint32_t previous
) noexcept
{
#if defined(PI_CRC32C_HAS_SSE42)
    if (hasSse42()) return calculateSse42(bytes, previous);
#endif

    std::uint32_t checksum = previous ^ 0xFFFFFFFFU;

    for (const std::uint8_t byte : bytes)
    {
        const std::uint8_t index = static_cast<std::uint8_t>(checksum ^ byte);
        checksum = crcTable[index] ^ (checksum >> 8);
    }

    return checksum ^ 0xFFFFFFFFU;
}

} // namespace pi::checkpoint
