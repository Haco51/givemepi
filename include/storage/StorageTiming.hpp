#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace pi::storage
{

/// Thread-safe aggregate timing counters for one storage benchmark population.
struct StorageTiming
{
    std::atomic<std::uint64_t> storeGmpEncodeNs{0};
    std::atomic<std::uint64_t> storeCompressionNs{0};
    std::atomic<std::uint64_t> storeFileWriteNs{0};
    std::atomic<std::uint64_t> storeFileSyncNs{0};
    std::atomic<std::uint64_t> storeRenameNs{0};
    std::atomic<std::uint64_t> storeDirectorySyncNs{0};
    std::atomic<std::uint64_t> storeIndexPublishNs{0};
    std::atomic<std::uint64_t> storeIndexMutexWaitNs{0};
    std::atomic<std::uint64_t> storeIndexCommitNs{0};
    std::atomic<std::uint64_t> loadFileReadNs{0};
    std::atomic<std::uint64_t> loadCrcNs{0};
    std::atomic<std::uint64_t> loadDecompressionNs{0};
    std::atomic<std::uint64_t> loadGmpDecodeNs{0};

    template <typename Rep, typename Period>
    static void add(
        std::atomic<std::uint64_t>& target,
        std::chrono::duration<Rep, Period> duration
    ) noexcept
    {
        target.fetch_add(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
                    .count()),
            std::memory_order_relaxed
        );
    }
};

} // namespace pi::storage
