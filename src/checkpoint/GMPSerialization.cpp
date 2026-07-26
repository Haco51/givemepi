#include "checkpoint/GMPSerialization.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::checkpoint
{

SerializedGMPInteger GMPSerialization::encode(
    const bigint::GMPInteger& value
)
{
    const int rawSign = mpz_sgn(*value.raw());

    if (rawSign == 0)
    {
        return SerializedGMPInteger{IntegerSign::zero, {}};
    }

    const auto exportMagnitude = [](const mpz_t* source)
    {
        const std::size_t bitCount = mpz_sizeinbase(*source, 2);

        if (bitCount > std::numeric_limits<std::size_t>::max() - 7)
            throw std::overflow_error("Canonical GMP magnitude size overflow");

        std::vector<std::uint8_t> magnitude((bitCount + 7) / 8);
        std::size_t written = 0;
        mpz_export(magnitude.data(), &written, 1, 1, 1, 0, *source);
        magnitude.resize(written);
        return magnitude;
    };

    std::vector<std::uint8_t> magnitude;
    if (rawSign > 0)
    {
        // mpz_export already emits the absolute magnitude for a non-negative
        // value, so avoid copying the GMP limb array on the hot path.
        magnitude = exportMagnitude(value.raw());
    }
    else
    {
        bigint::GMPInteger absolute(value);
        absolute.negate();
        magnitude = exportMagnitude(absolute.raw());
    }

    return SerializedGMPInteger{
        rawSign > 0 ? IntegerSign::positive : IntegerSign::negative,
        std::move(magnitude)
    };
}


bigint::GMPInteger GMPSerialization::decode(
    IntegerSign sign,
    std::span<const std::uint8_t> magnitude
)
{
    switch (sign)
    {
        case IntegerSign::zero:
            if (!magnitude.empty())
            {
                throw std::invalid_argument(
                    "Canonical zero must have an empty magnitude"
                );
            }

            return bigint::GMPInteger{};

        case IntegerSign::positive:
        case IntegerSign::negative:
            if (magnitude.empty())
            {
                throw std::invalid_argument(
                    "Canonical non-zero integer requires a magnitude"
                );
            }

            if (magnitude.front() == 0)
            {
                throw std::invalid_argument(
                    "Canonical magnitude must not contain a leading zero"
                );
            }

            break;

        default:
            throw std::invalid_argument(
                "Unknown canonical integer sign"
            );
    }

    bigint::GMPInteger value;

    mpz_import(
        *value.raw(),
        magnitude.size(),
        1,
        1,
        1,
        0,
        magnitude.data()
    );

    if (sign == IntegerSign::negative)
    {
        value.negate();
    }

    return value;
}

} // namespace pi::checkpoint
