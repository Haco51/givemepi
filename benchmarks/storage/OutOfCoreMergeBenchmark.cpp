#include "binary/BinarySplitter.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "scheduler/Scheduler.hpp"
#include "storage/StorageMergeCoordinator.hpp"

#include <chrono>
#include <cstdint>
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
        if (digits == 0 || budgetMiB == 0)
        {
            throw std::invalid_argument("digits and budget must be positive");
        }

        const auto precision = PrecisionPolicy::create(digits);
        const auto computation =
            ComputationIdentity::fromPrecisionPlan(precision);
        StoragePolicy policy;
        policy.directory = std::filesystem::temp_directory_path()
            / ("givemepi-pr0026-merge-benchmark-"
               + std::to_string(::getpid()));
        policy.memory_budget_bytes = budgetMiB * 1024ULL * 1024ULL;
        policy.target_chunk_size_bytes = std::min<std::uint64_t>(
            policy.memory_budget_bytes, 64ULL * 1024ULL * 1024ULL);

        std::error_code ec;
        std::filesystem::remove_all(policy.directory, ec);
        StorageManager manager(policy);
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
                  << " digits=" << digits
                  << " terms=" << precision.termCount
                  << " elapsed_seconds=" << elapsed
                  << " peak_rss_mib=" << peakRssMiB()
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
