#pragma once

#include "storage/ChunkCodec.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace pi::storage
{

class StorageManager;
struct AsyncReadOperation;

enum class AsyncReadState
{
    queued,
    loading,
    loaded,
    failed,
    cancelled
};

class AsyncReadHandle
{
public:
    AsyncReadHandle() noexcept = default;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] AsyncReadState state() const noexcept;
    [[nodiscard]] std::optional<std::string> error() const;
    [[nodiscard]] std::optional<Chunk> takeChunk();
    void wait() const;

private:
    explicit AsyncReadHandle(std::shared_ptr<AsyncReadOperation> operation)
        : operation_(std::move(operation))
    {
    }

    friend class AsyncChunkReader;
    std::shared_ptr<AsyncReadOperation> operation_;
};

class AsyncChunkReader
{
public:
    AsyncChunkReader(
        const StorageManager& manager,
        std::size_t queueCapacity,
        std::size_t workerCount = 1
    );
    ~AsyncChunkReader();

    AsyncChunkReader(const AsyncChunkReader&) = delete;
    AsyncChunkReader& operator=(const AsyncChunkReader&) = delete;

    [[nodiscard]] AsyncReadHandle loadAsync(const ChunkIdentity& identity);
    void waitForCapacity();
    void shutdown(bool drain = true);
    [[nodiscard]] std::size_t queuedCount() const noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::uint64_t completedCount() const noexcept;
    [[nodiscard]] std::uint64_t failedCount() const noexcept;

private:
    void workerLoop();

    const StorageManager& manager_;
    const std::size_t capacity_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueChanged_;
    std::deque<std::shared_ptr<AsyncReadOperation>> queue_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> activeCount_{0};
    std::atomic<std::uint64_t> completedCount_{0};
    std::atomic<std::uint64_t> failedCount_{0};
    std::uint64_t nextRequestId_ = 1;
    bool stopping_ = false;
};

} // namespace pi::storage
