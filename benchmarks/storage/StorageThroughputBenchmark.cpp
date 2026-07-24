#include "bigint/GMPInteger.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "storage/StorageManager.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

std::uint64_t parseTargetMiB(int argc, char* argv[])
{
    if (argc < 2) return 100;
    const auto value = std::stoull(argv[1]);
    if (value == 0) throw std::invalid_argument("target MiB must be > 0");
    return value;
}

pi::storage::CompressionAlgorithm parseCompression(int argc, char* argv[])
{
    if (argc < 3) return pi::storage::CompressionAlgorithm::none;
    return pi::storage::parseCompressionAlgorithm(argv[2]);
}

double seconds(std::chrono::steady_clock::duration duration)
{
    return std::chrono::duration<double>(duration).count();
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    try
    {
        const std::uint64_t targetMiB = parseTargetMiB(argc, argv);
        const auto compression = parseCompression(argc, argv);
        const std::uint64_t targetBytes = targetMiB * 1024ULL * 1024ULL;
        // Three values of 10^digits occupy approximately 1.25 * digits bytes
        // after GMP export. Add a margin so the measured payload is near target.
        const std::uint64_t decimalDigits =
            std::max<std::uint64_t>(1000, targetBytes * 8 / 10);
        const auto plan = PrecisionPolicy::create(decimalDigits);
        const auto computation =
            ComputationIdentity::fromPrecisionPlan(plan);
        const auto identity = ChunkIdentity::create(
            computation,
            BlockLocation::create(0, 4, 0, computation));

        pi::bigint::GMPInteger p;
        p.setPowerOfTen(decimalDigits);
        pi::bigint::GMPInteger q(p);
        q.sub(pi::bigint::GMPInteger(1));
        pi::bigint::GMPInteger t(p);
        t.add(pi::bigint::GMPInteger(1));
        const Chunk chunk{
            ChunkCodec::createMetadata(identity, p, q, t, compression),
            p, q, t};

        const auto directory = std::filesystem::temp_directory_path()
            / ("givemepi-storage-throughput-" + std::to_string(::getpid()));
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        StoragePolicy policy;
        policy.directory = directory;
        policy.memory_budget_bytes = std::max<std::uint64_t>(
            targetBytes * 2, defaultMemoryBudgetBytes);
        policy.compression = compression;

        const auto started = std::chrono::steady_clock::now();
        StorageManager manager(policy);
        const auto storeStarted = std::chrono::steady_clock::now();
        const ChunkId id = manager.store(chunk);
        const auto storeFinished = std::chrono::steady_clock::now();
        const auto loaded = manager.load(identity);
        const auto loadFinished = std::chrono::steady_clock::now();

        if (!loaded.has_value() || loaded->p.compare(p) != 0
            || loaded->q.compare(q) != 0 || loaded->t.compare(t) != 0)
        {
            std::cerr << "large chunk round-trip mismatch\n";
            return 1;
        }

        const auto snapshot = manager.snapshot();
        const double storeSeconds = seconds(storeFinished - storeStarted);
        const double loadSeconds = seconds(loadFinished - storeFinished);
        const double totalSeconds = seconds(loadFinished - started);
        const auto mib = [](std::uint64_t bytes) {
            return static_cast<double>(bytes) / (1024.0 * 1024.0);
        };
        std::cout << std::fixed << std::setprecision(2)
                  << "target_mib=" << targetMiB
                  << " compression=" << toString(compression)
                  << " encoded_mib=" << mib(chunk.metadata.storedSize)
                  << " store_seconds=" << storeSeconds
                  << " store_mib_per_second="
                  << mib(chunk.metadata.storedSize) / storeSeconds
                  << " load_seconds=" << loadSeconds
                  << " load_mib_per_second="
                  << mib(chunk.metadata.storedSize) / loadSeconds
                  << " total_seconds=" << totalSeconds
                  << " indexed_chunks=" << snapshot.indexedChunks
                  << " chunk_id=" << id << '\n';

        std::filesystem::remove_all(directory, ec);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "storage throughput benchmark failed: " << error.what()
                  << '\n';
        return 1;
    }
}
