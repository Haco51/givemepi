#include "checkpoint/CheckpointTypes.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "storage/AsyncReader.hpp"
#include "storage/AsyncWriter.hpp"
#include "storage/ChunkCodec.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageTiming.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{

struct Options
{
    std::size_t chunks = 8;
    unsigned long bits = 1'000'000;
    std::size_t workers = 4;
    std::size_t queueCapacity = 4;
    pi::storage::CompressionAlgorithm compression =
        pi::storage::CompressionAlgorithm::none;
    std::filesystem::path directory;
};

Options parseOptions(int argc, char* argv[])
{
    Options options;
    if (argc >= 2) options.chunks = std::stoull(argv[1]);
    if (argc >= 3) options.bits = std::stoul(argv[2]);
    if (argc >= 4) options.workers = std::stoull(argv[3]);
    if (argc >= 5) options.queueCapacity = std::stoull(argv[4]);
    if (argc >= 6)
    {
        const auto compression = std::string_view(argv[5]);
        if (compression == "none")
            options.compression = pi::storage::CompressionAlgorithm::none;
        else if (compression == "lz4")
            options.compression = pi::storage::CompressionAlgorithm::lz4;
        else
            throw std::invalid_argument("compression must be none or lz4");
    }
    options.directory = argc >= 7
        ? argv[6]
        : std::filesystem::temp_directory_path()
            / ("givemepi-pr0029-reader-writer-" + std::to_string(::getpid()));
    if (options.chunks == 0 || options.bits < 16
        || options.workers == 0 || options.queueCapacity == 0)
        throw std::invalid_argument("benchmark arguments must be positive");
    return options;
}

pi::storage::Chunk makeChunk(
    const pi::checkpoint::ComputationIdentity& computation,
    std::size_t index,
    unsigned long bits,
    pi::storage::CompressionAlgorithm compression
)
{
    using namespace pi::checkpoint;
    using namespace pi::storage;
    const auto identity = ChunkIdentity::create(
        computation,
        BlockLocation::create(
            static_cast<std::uint64_t>(index),
            static_cast<std::uint64_t>(index + 1),
            0,
            computation));
    pi::bigint::GMPInteger p;
    pi::bigint::GMPInteger q;
    pi::bigint::GMPInteger t;
    mpz_ui_pow_ui(*p.raw(), 2, bits);
    mpz_ui_pow_ui(*q.raw(), 2, bits - 1);
    mpz_ui_pow_ui(*t.raw(), 2, bits - 2);
    return Chunk{
        ChunkCodec::createMetadata(identity, p, q, t, compression),
        std::move(p), std::move(q), std::move(t)};
}

double milliseconds(std::uint64_t nanoseconds)
{
    return static_cast<double>(nanoseconds) / 1'000'000.0;
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    try
    {
        const Options options = parseOptions(argc, argv);
        const auto precision = PrecisionPolicy::create(1'000);
        const auto computation =
            ComputationIdentity::fromPrecisionPlan(precision);

        std::error_code error;
        std::filesystem::remove_all(options.directory, error);
        StoragePolicy policy;
        policy.directory = options.directory;
        policy.compression = options.compression;
        policy.memory_budget_bytes = 2ULL * 1024 * 1024 * 1024;
        policy.target_chunk_size_bytes = 64ULL * 1024 * 1024;
        StorageTiming timing;
        StorageManager manager(policy, &timing);

        std::vector<Chunk> writerChunks;
        std::vector<ChunkIdentity> readerIdentities;
        std::uint64_t bytesPerSide = 0;
        writerChunks.reserve(options.chunks);
        readerIdentities.reserve(options.chunks);
        for (std::size_t index = 0; index < options.chunks; ++index)
        {
            auto readerChunk = makeChunk(
                computation, index, options.bits, options.compression);
            readerIdentities.push_back(readerChunk.metadata.identity);
            bytesPerSide += readerChunk.metadata.storedSize;
            static_cast<void>(manager.store(readerChunk));
            auto writerChunk = makeChunk(
                computation,
                options.chunks + index,
                options.bits,
                options.compression);
            bytesPerSide += writerChunk.metadata.storedSize;
            writerChunks.push_back(std::move(writerChunk));
        }

        AsyncChunkWriter writer(manager, options.queueCapacity, options.workers);
        AsyncChunkReader reader(manager, options.queueCapacity, options.workers);
        std::atomic<bool> readyWriter{false};
        std::atomic<bool> readyReader{false};
        std::atomic<bool> go{false};
        std::atomic<std::size_t> writerSuccess{0};
        std::atomic<std::size_t> readerSuccess{0};
        std::mutex errorMutex;
        std::string failure;

        auto recordFailure = [&](const std::string& detail)
        {
            std::lock_guard lock(errorMutex);
            if (failure.empty()) failure = detail;
        };

        std::thread writerThread([&]
        {
            readyWriter.store(true, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            try
            {
                std::vector<AsyncWriteHandle> handles;
                handles.reserve(writerChunks.size());
                for (auto& chunk : writerChunks)
                {
                    writer.waitForCapacity();
                    handles.push_back(writer.submit(std::move(chunk)));
                }
                for (const auto& handle : handles)
                {
                    handle.wait();
                    if (handle.state() != AsyncWriteState::stored)
                        throw std::runtime_error(
                            handle.error().value_or("writer request failed"));
                    ++writerSuccess;
                }
            }
            catch (const std::exception& exception)
            {
                recordFailure(exception.what());
            }
        });

        std::thread readerThread([&]
        {
            readyReader.store(true, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            try
            {
                std::vector<AsyncReadHandle> handles;
                handles.reserve(readerIdentities.size());
                for (const auto& identity : readerIdentities)
                {
                    reader.waitForCapacity();
                    handles.push_back(reader.loadAsync(identity));
                }
                for (auto& handle : handles)
                {
                    handle.wait();
                    if (handle.state() != AsyncReadState::loaded
                        || !handle.takeChunk().has_value())
                        throw std::runtime_error(
                            handle.error().value_or("reader request failed"));
                    ++readerSuccess;
                }
            }
            catch (const std::exception& exception)
            {
                recordFailure(exception.what());
            }
        });

        while (!readyWriter.load(std::memory_order_acquire)
            || !readyReader.load(std::memory_order_acquire))
            std::this_thread::yield();
        const auto started = std::chrono::steady_clock::now();
        go.store(true, std::memory_order_release);
        writerThread.join();
        readerThread.join();
        writer.shutdown();
        reader.shutdown();
        const auto finished = std::chrono::steady_clock::now();
        if (!failure.empty()) throw std::runtime_error(failure);

        const double elapsed =
            std::chrono::duration<double>(finished - started).count();
        const auto snapshot = manager.snapshot();
        const auto bytesForOneSide = bytesPerSide / 2;
        std::cout << std::fixed << std::setprecision(3)
                  << "mode=independent-reader-writer"
                  << " compression=" << (options.compression == CompressionAlgorithm::lz4 ? "lz4" : "none")
                  << " chunks_per_side=" << options.chunks
                  << " bits_per_gmp=" << options.bits
                  << " workers=" << options.workers
                  << " queue_capacity=" << options.queueCapacity
                  << " elapsed_seconds=" << elapsed
                  << " writer_success=" << writerSuccess.load()
                  << " reader_success=" << readerSuccess.load()
                  << " writer_mib_per_second="
                  << static_cast<double>(bytesForOneSide) / (1024.0 * 1024.0) / elapsed
                  << " reader_mib_per_second="
                  << static_cast<double>(bytesForOneSide) / (1024.0 * 1024.0) / elapsed
                  << " writer_wait_ms=" << milliseconds(writer.capacityWaitNs())
                  << " writer_wait_count=" << writer.capacityWaitCount()
                  << " writer_active_ms=" << milliseconds(writer.activeNs())
                  << " reader_wait_ms=" << milliseconds(reader.capacityWaitNs())
                  << " reader_wait_count=" << reader.capacityWaitCount()
                  << " reader_active_ms=" << milliseconds(reader.activeNs())
                  << " stored_bytes=" << snapshot.storedBytes
                  << " indexed_chunks=" << snapshot.indexedChunks
                  << '\n';

        std::filesystem::remove_all(options.directory, error);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "reader-writer concurrency benchmark failed: "
                  << exception.what() << '\n';
        return 1;
    }
}
