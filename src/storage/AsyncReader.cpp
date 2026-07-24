#include "storage/AsyncReader.hpp"

#include "storage/StorageManager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pi::storage
{

struct AsyncReadOperation
{
    explicit AsyncReadOperation(
        std::uint64_t id,
        ChunkIdentity value
    )
        : requestId(id),
          identity(std::move(value))
    {
    }

    mutable std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t requestId;
    ChunkIdentity identity;
    AsyncReadState state = AsyncReadState::queued;
    std::optional<Chunk> chunk;
    std::optional<std::string> error;
};

bool AsyncReadHandle::valid() const noexcept
{
    return operation_ != nullptr;
}

AsyncReadState AsyncReadHandle::state() const noexcept
{
    if (!operation_) return AsyncReadState::cancelled;
    std::lock_guard lock(operation_->mutex);
    return operation_->state;
}

std::optional<std::string> AsyncReadHandle::error() const
{
    if (!operation_) return std::nullopt;
    std::lock_guard lock(operation_->mutex);
    return operation_->error;
}

std::optional<Chunk> AsyncReadHandle::takeChunk()
{
    if (!operation_) return std::nullopt;
    std::lock_guard lock(operation_->mutex);
    if (operation_->state != AsyncReadState::loaded) return std::nullopt;
    return std::move(operation_->chunk);
}

void AsyncReadHandle::wait() const
{
    if (!operation_) return;
    std::unique_lock lock(operation_->mutex);
    operation_->changed.wait(lock, [this]() {
        return operation_->state == AsyncReadState::loaded
            || operation_->state == AsyncReadState::failed
            || operation_->state == AsyncReadState::cancelled;
    });
}

AsyncChunkReader::AsyncChunkReader(
    const StorageManager& manager,
    std::size_t queueCapacity,
    std::size_t workerCount
)
    : manager_(manager),
      capacity_(queueCapacity)
{
    if (capacity_ == 0 || workerCount == 0)
    {
        throw std::invalid_argument(
            "Async reader capacity and worker count must be positive");
    }
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index)
    {
        workers_.emplace_back(&AsyncChunkReader::workerLoop, this);
    }
}

AsyncChunkReader::~AsyncChunkReader()
{
    shutdown(true);
}

AsyncReadHandle AsyncChunkReader::loadAsync(const ChunkIdentity& identity)
{
    std::lock_guard lock(queueMutex_);
    if (stopping_) throw std::runtime_error("Async reader is stopped");
    if (queue_.size() >= capacity_)
    {
        throw std::runtime_error("Async reader queue is full");
    }
    auto operation = std::make_shared<AsyncReadOperation>(
        nextRequestId_++, identity);
    queue_.push_back(operation);
    queueChanged_.notify_one();
    return AsyncReadHandle(std::move(operation));
}

void AsyncChunkReader::waitForCapacity()
{
    std::unique_lock lock(queueMutex_);
    queueChanged_.wait(lock, [this]() {
        return stopping_ || queue_.size() < capacity_;
    });
    if (stopping_) throw std::runtime_error("Async reader is stopped");
}

std::size_t AsyncChunkReader::queuedCount() const noexcept
{
    std::lock_guard lock(queueMutex_);
    return queue_.size();
}

std::size_t AsyncChunkReader::activeCount() const noexcept
{
    return activeCount_.load(std::memory_order_relaxed);
}

std::uint64_t AsyncChunkReader::completedCount() const noexcept
{
    return completedCount_.load(std::memory_order_relaxed);
}

std::uint64_t AsyncChunkReader::failedCount() const noexcept
{
    return failedCount_.load(std::memory_order_relaxed);
}

void AsyncChunkReader::shutdown(bool drain)
{
    {
        std::lock_guard lock(queueMutex_);
        if (stopping_ && workers_.empty()) return;
        stopping_ = true;
        if (!drain)
        {
            while (!queue_.empty())
            {
                const auto operation = queue_.front();
                queue_.pop_front();
                {
                    std::lock_guard operationLock(operation->mutex);
                    operation->state = AsyncReadState::cancelled;
                }
                operation->changed.notify_all();
            }
        }
    }
    queueChanged_.notify_all();
    for (auto& worker : workers_)
    {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

void AsyncChunkReader::workerLoop()
{
    while (true)
    {
        std::shared_ptr<AsyncReadOperation> operation;
        {
            std::unique_lock lock(queueMutex_);
            queueChanged_.wait(lock, [this]() {
                return stopping_ || !queue_.empty();
            });
            if (queue_.empty())
            {
                if (stopping_) return;
                continue;
            }
            operation = std::move(queue_.front());
            queue_.pop_front();
        }
        queueChanged_.notify_all();
        activeCount_.fetch_add(1, std::memory_order_relaxed);

        {
            std::lock_guard lock(operation->mutex);
            operation->state = AsyncReadState::loading;
        }
        try
        {
            std::optional<Chunk> loaded;
            {
                loaded = manager_.load(operation->identity);
            }
            if (!loaded.has_value())
            {
                throw std::runtime_error("async reload found no indexed chunk");
            }
            {
                std::lock_guard lock(operation->mutex);
                operation->chunk = std::move(loaded);
                operation->state = AsyncReadState::loaded;
            }
        }
        catch (const std::exception& error)
        {
            std::lock_guard lock(operation->mutex);
            operation->error = error.what();
            operation->state = AsyncReadState::failed;
        }
        catch (...)
        {
            std::lock_guard lock(operation->mutex);
            operation->error = "Unknown asynchronous reload failure";
            operation->state = AsyncReadState::failed;
        }
        {
            std::lock_guard lock(operation->mutex);
            if (operation->state == AsyncReadState::loaded)
            {
                completedCount_.fetch_add(1, std::memory_order_relaxed);
            }
            else if (operation->state == AsyncReadState::failed)
            {
                failedCount_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        activeCount_.fetch_sub(1, std::memory_order_relaxed);
        operation->changed.notify_all();
    }
}

} // namespace pi::storage
