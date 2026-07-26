#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace pi::storage
{
struct AsyncWriteOperation;
class StorageManager;
struct Chunk;

enum class AsyncWriteState
{
    queued,
    writing,
    stored,
    failed,
    cancelled
};

[[nodiscard]]
std::string_view toString(AsyncWriteState state) noexcept;

/// Lifecycle contract for one future asynchronous chunk write.
///
/// This type owns no payload and performs no queue or filesystem operation.
/// The eventual writer must retain the caller's chunk until `stored` is
/// reached; only `stored` establishes a durable, indexed copy.
class AsyncWriteLifecycle
{
public:
    explicit AsyncWriteLifecycle(std::uint64_t requestId) noexcept;

    [[nodiscard]] std::uint64_t requestId() const noexcept;
    [[nodiscard]] AsyncWriteState state() const noexcept;
    [[nodiscard]] bool hasDurableCopy() const noexcept;
    [[nodiscard]] const std::optional<std::string>& error() const noexcept;

    [[nodiscard]] bool beginWriting() noexcept;
    [[nodiscard]] bool completeStored() noexcept;
    [[nodiscard]] bool fail(std::string detail);
    /// Only queued requests may be cancelled; active writes drain or fail.
    [[nodiscard]] bool cancel() noexcept;

private:
    std::uint64_t requestId_;
    AsyncWriteState state_ = AsyncWriteState::queued;
    std::optional<std::string> error_;
};

/// Read-only handle for one submitted asynchronous write.
class AsyncWriteHandle
{
public:
    AsyncWriteHandle() noexcept = default;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::uint64_t requestId() const noexcept;
    [[nodiscard]] AsyncWriteState state() const noexcept;
    [[nodiscard]] bool hasDurableCopy() const noexcept;
    [[nodiscard]] std::optional<std::string> error() const;

    /// Waits until the request reaches stored, failed, or cancelled.
    void wait() const;

private:
    explicit AsyncWriteHandle(std::shared_ptr<AsyncWriteOperation> operation)
        : operation_(std::move(operation))
    {
    }

    friend class AsyncChunkWriter;
    std::shared_ptr<AsyncWriteOperation> operation_;
};

/// Bounded asynchronous facade over the synchronous StorageManager.
class AsyncChunkWriter
{
public:
    AsyncChunkWriter(
        StorageManager& manager,
        std::size_t queueCapacity,
        std::size_t workerCount = 1
    );
    ~AsyncChunkWriter();

    AsyncChunkWriter(const AsyncChunkWriter&) = delete;
    AsyncChunkWriter& operator=(const AsyncChunkWriter&) = delete;

    /// Submits a request or throws when the bounded queue is full/stopped.
    [[nodiscard]] AsyncWriteHandle submit(Chunk chunk);

    /// Blocks until a queue slot is available or the writer is stopped.
    void waitForCapacity();

    /// Cancels a queued request. Active writes cannot be cancelled.
    [[nodiscard]] bool cancel(const AsyncWriteHandle& handle) noexcept;

    /// Drain accepted requests, or cancel queued requests when drain=false.
    void shutdown(bool drain = true);

    [[nodiscard]] std::size_t queuedCount() const noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::uint64_t completedCount() const noexcept;
    [[nodiscard]] std::uint64_t failedCount() const noexcept;
    /// Aggregate time spent waiting for queue capacity.
    [[nodiscard]] std::uint64_t capacityWaitNs() const noexcept;
    /// Number of capacity wait observations.
    [[nodiscard]] std::uint64_t capacityWaitCount() const noexcept;
    /// Aggregate time spent processing accepted write operations.
    [[nodiscard]] std::uint64_t activeNs() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    void workerLoop();

    StorageManager& manager_;
    const std::size_t capacity_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueChanged_;
    std::deque<std::shared_ptr<AsyncWriteOperation>> queue_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> activeCount_{0};
    std::atomic<std::uint64_t> completedCount_{0};
    std::atomic<std::uint64_t> failedCount_{0};
    std::atomic<std::uint64_t> capacityWaitNs_{0};
    std::atomic<std::uint64_t> capacityWaitCount_{0};
    std::atomic<std::uint64_t> activeNs_{0};
    std::uint64_t nextRequestId_ = 1;
    bool stopping_ = false;
};

} // namespace pi::storage
