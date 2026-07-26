#include "binary/BinarySplitter.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "scheduler/Scheduler.hpp"
#include "storage/StorageMergeCoordinator.hpp"
#include "storage/StorageTiming.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>

namespace
{

std::uint64_t peakRssMiB()
{
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0)
    {
        return 0;
    }
    return static_cast<std::uint64_t>(usage.ru_maxrss) / 1024;
}

double milliseconds(std::uint64_t nanoseconds)
{
    return static_cast<double>(nanoseconds) / 1'000'000.0;
}

std::uint64_t parseDigits(int argc, char* argv[])
{
    if (argc < 2) return 10'000;
    return std::stoull(argv[1]);
}

std::uint64_t parseBudgetMiB(int argc, char* argv[])
{
    if (argc < 3) return 1;
    return std::stoull(argv[2]);
}

bool outOfCore(int argc, char* argv[])
{
    return argc < 4 || std::string_view(argv[3]) == "out-of-core";
}

bool asynchronousIo(int argc, char* argv[])
{
    return argc >= 5 && std::string_view(argv[4]) == "async";
}

std::size_t ioWorkers(int argc, char* argv[], bool useAsync)
{
    if (!useAsync || argc < 6) return 1;
    return std::max<std::size_t>(1, std::stoull(argv[5]));
}

std::size_t ioQueueCapacity(int argc, char* argv[], std::size_t workers)
{
    if (argc < 7) return std::max<std::size_t>(1, workers * 8);
    return std::max<std::size_t>(1, std::stoull(argv[6]));
}

std::filesystem::path storageDirectory(int argc, char* argv[])
{
    if (argc >= 8) return argv[7];
    return std::filesystem::temp_directory_path()
        / ("givemepi-pr0026-merge-benchmark-"
           + std::to_string(::getpid()));
}

pi::storage::CompressionAlgorithm compressionAlgorithm(int argc, char* argv[])
{
    if (argc < 9) return pi::storage::CompressionAlgorithm::none;
    return pi::storage::parseCompressionAlgorithm(argv[8]);
}

std::string_view cacheMode(int argc, char* argv[])
{
    if (argc < 10) return "warm";
    const auto mode = std::string_view(argv[9]);
    if (mode != "cold" && mode != "warm")
        throw std::invalid_argument("cache mode must be cold or warm");
    return mode;
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace pi::binary;
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    try
    {
        const std::uint64_t digits = parseDigits(argc, argv);
        const std::uint64_t budgetMiB = parseBudgetMiB(argc, argv);
        const bool useStorage = outOfCore(argc, argv);
        const bool useAsyncIo = useStorage && asynchronousIo(argc, argv);
        const std::size_t workers = ioWorkers(argc, argv, useAsyncIo);
        const std::size_t queueCapacity =
            ioQueueCapacity(argc, argv, workers);
        const auto compression = compressionAlgorithm(argc, argv);
        const auto cache = cacheMode(argc, argv);
        if (digits == 0 || budgetMiB == 0)
        {
            throw std::invalid_argument("digits and budget must be positive");
        }

        const auto precision = PrecisionPolicy::create(digits);
        const auto computation =
            ComputationIdentity::fromPrecisionPlan(precision);
        StoragePolicy policy;
        policy.directory = storageDirectory(argc, argv);
        policy.memory_budget_bytes = budgetMiB * 1024ULL * 1024ULL;
        policy.target_chunk_size_bytes = std::min<std::uint64_t>(
            policy.memory_budget_bytes, 64ULL * 1024ULL * 1024ULL);
        policy.compression = compression;
        if (::setenv("PI_STORAGE_CACHE_MODE", std::string(cache).c_str(), 1)
            != 0)
            throw std::runtime_error("cannot set storage cache mode");

        std::error_code ec;
        std::filesystem::remove_all(policy.directory, ec);
        StorageTiming timing;
        StorageManager manager(policy, &timing);
        std::optional<AsyncChunkWriter> asyncWriter;
        std::optional<AsyncChunkReader> asyncReader;
        if (useAsyncIo)
        {
            asyncWriter.emplace(manager, queueCapacity, workers);
            asyncReader.emplace(manager, queueCapacity, workers);
        }
        StorageMergeCoordinator coordinator(
            manager,
            computation,
            nullptr,
            useAsyncIo ? &*asyncWriter : nullptr,
            useAsyncIo ? &*asyncReader : nullptr);

        const auto started = std::chrono::steady_clock::now();
        pi::scheduler::Scheduler scheduler(4, 256);
        scheduler.start();
        const auto result = BinarySplitter::splitParallel(
            0,
            precision.termCount,
            scheduler,
            ParallelSplitOptions{64, 4},
            nullptr,
            useStorage ? &coordinator : nullptr);
        scheduler.stop();
        const auto finished = std::chrono::steady_clock::now();

        const double elapsed =
            std::chrono::duration<double>(finished - started).count();
        const auto snapshot = manager.snapshot();
        std::cout << std::fixed << std::setprecision(3)
                  << "mode=" << (useStorage ? "out-of-core" : "in-memory")
                  << " io=" << (useAsyncIo ? "async" : "sync")
                  << " io_workers=" << workers
                  << " io_queue_capacity=" << queueCapacity
                  << " compression=" << toString(compression)
                  << " cache_mode=" << cache
                  << " digits=" << digits
                  << " terms=" << precision.termCount
                  << " elapsed_seconds=" << elapsed
                  << " peak_rss_mib=" << peakRssMiB()
                  << " store_gmp_encode_ms="
                  << milliseconds(timing.storeGmpEncodeNs.load())
                  << " store_compression_ms="
                  << milliseconds(timing.storeCompressionNs.load())
                  << " store_file_write_ms="
                  << milliseconds(timing.storeFileWriteNs.load())
                  << " store_file_sync_ms="
                  << milliseconds(timing.storeFileSyncNs.load())
                  << " store_rename_ms="
                  << milliseconds(timing.storeRenameNs.load())
                  << " store_directory_sync_ms="
                  << milliseconds(timing.storeDirectorySyncNs.load())
                  << " store_index_publish_ms="
                  << milliseconds(timing.storeIndexPublishNs.load())
                  << " store_index_mutex_wait_ms="
                  << milliseconds(timing.storeIndexMutexWaitNs.load())
                  << " store_index_commit_ms="
                  << milliseconds(timing.storeIndexCommitNs.load())
                  << " load_file_read_ms="
                  << milliseconds(timing.loadFileReadNs.load())
                  << " load_crc_ms="
                  << milliseconds(timing.loadCrcNs.load())
                  << " load_decompression_ms="
                  << milliseconds(timing.loadDecompressionNs.load())
                  << " load_gmp_decode_ms="
                  << milliseconds(timing.loadGmpDecodeNs.load())
                  << " writer_wait_ms="
                  << (asyncWriter.has_value()
                          ? milliseconds(asyncWriter->capacityWaitNs())
                          : 0.0)
                  << " writer_wait_count="
                  << (asyncWriter.has_value()
                          ? asyncWriter->capacityWaitCount()
                          : 0)
                  << " writer_active_ms="
                  << (asyncWriter.has_value()
                          ? milliseconds(asyncWriter->activeNs())
                          : 0.0)
                  << " reader_wait_ms="
                  << (asyncReader.has_value()
                          ? milliseconds(asyncReader->capacityWaitNs())
                          : 0.0)
                  << " reader_wait_count="
                  << (asyncReader.has_value()
                          ? asyncReader->capacityWaitCount()
                          : 0)
                  << " reader_active_ms="
                  << (asyncReader.has_value()
                          ? milliseconds(asyncReader->activeNs())
                          : 0.0)
                  << " spill_count=" << coordinator.spillCount()
                  << " reload_count=" << coordinator.reloadCount()
                  << " spilled_bytes=" << coordinator.spilledBytes()
                  << " stored_bytes=" << snapshot.storedBytes
                  << " resident_bytes=" << snapshot.residentBytes
                  << " result_p_bits="
                  << mpz_sizeinbase(*result.P().raw(), 2)
                  << '\n';

        std::filesystem::remove_all(policy.directory, ec);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "out-of-core merge benchmark failed: " << error.what()
                  << '\n';
        return 1;
    }
}
