#include "storage/AsyncWriter.hpp"

#include "storage/ChunkCodec.hpp"
#include "storage/StorageManager.hpp"

#include <algorithm>
#include <condition_variable>
#include <stdexcept>
#include <utility>

namespace pi::storage
{

struct AsyncWriteOperation
{
    explicit AsyncWriteOperation(std::uint64_t id, Chunk value)
        : lifecycle(id),
          chunk(std::move(value))
    {
    }

    mutable std::mutex mutex;
    std::condition_variable changed;
    AsyncWriteLifecycle lifecycle;
    std::optional<Chunk> chunk;
};

std::string_view toString(AsyncWriteState state) noexcept
{
    switch (state)
    {
        case AsyncWriteState::queued: return "queued";
        case AsyncWriteState::writing: return "writing";
        case AsyncWriteState::stored: return "stored";
        case AsyncWriteState::failed: return "failed";
        case AsyncWriteState::cancelled: return "cancelled";
    }
    return "unknown";
}

AsyncWriteLifecycle::AsyncWriteLifecycle(std::uint64_t requestId) noexcept
    : requestId_(requestId)
{
}

std::uint64_t AsyncWriteLifecycle::requestId() const noexcept
{
    return requestId_;
}

AsyncWriteState AsyncWriteLifecycle::state() const noexcept
{
    return state_;
}

bool AsyncWriteLifecycle::hasDurableCopy() const noexcept
{
    return state_ == AsyncWriteState::stored;
}

const std::optional<std::string>& AsyncWriteLifecycle::error() const noexcept
{
    return error_;
}

bool AsyncWriteLifecycle::beginWriting() noexcept
{
    if (state_ != AsyncWriteState::queued) return false;
    state_ = AsyncWriteState::writing;
    return true;
}

bool AsyncWriteLifecycle::completeStored() noexcept
{
    if (state_ != AsyncWriteState::writing) return false;
    state_ = AsyncWriteState::stored;
    return true;
}

bool AsyncWriteLifecycle::fail(std::string detail)
{
    if (state_ != AsyncWriteState::queued
        && state_ != AsyncWriteState::writing)
    {
        return false;
    }
    error_ = std::move(detail);
    state_ = AsyncWriteState::failed;
    return true;
}

bool AsyncWriteLifecycle::cancel() noexcept
{
    if (state_ != AsyncWriteState::queued) return false;
    state_ = AsyncWriteState::cancelled;
    return true;
}

bool AsyncWriteHandle::valid() const noexcept
{
    return operation_ != nullptr;
}

std::uint64_t AsyncWriteHandle::requestId() const noexcept
{
    if (!operation_) return 0;
    std::lock_guard lock(operation_->mutex);
    return operation_->lifecycle.requestId();
}

AsyncWriteState AsyncWriteHandle::state() const noexcept
{
    if (!operation_) return AsyncWriteState::cancelled;
    std::lock_guard lock(operation_->mutex);
    return operation_->lifecycle.state();
}

bool AsyncWriteHandle::hasDurableCopy() const noexcept
{
    if (!operation_) return false;
    std::lock_guard lock(operation_->mutex);
    return operation_->lifecycle.hasDurableCopy();
}

std::optional<std::string> AsyncWriteHandle::error() const
{
    if (!operation_) return std::nullopt;
    std::lock_guard lock(operation_->mutex);
    return operation_->lifecycle.error();
}

void AsyncWriteHandle::wait() const
{
    if (!operation_) return;
    std::unique_lock lock(operation_->mutex);
    operation_->changed.wait(lock, [this]() {
        const auto state = operation_->lifecycle.state();
        return state == AsyncWriteState::stored
            || state == AsyncWriteState::failed
            || state == AsyncWriteState::cancelled;
    });
}

AsyncChunkWriter::AsyncChunkWriter(
    StorageManager& manager,
    std::size_t queueCapacity,
    std::size_t workerCount
)
    : manager_(manager),
      capacity_(queueCapacity)
{
    if (capacity_ == 0 || workerCount == 0)
    {
        throw std::invalid_argument(
            "Async writer capacity and worker count must be positive");
    }
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index)
    {
        workers_.emplace_back(&AsyncChunkWriter::workerLoop, this);
    }
}

AsyncChunkWriter::~AsyncChunkWriter()
{
    shutdown(true);
}

AsyncWriteHandle AsyncChunkWriter::submit(Chunk chunk)
{
    std::shared_ptr<AsyncWriteOperation> operation;
    {
        std::lock_guard lock(queueMutex_);
        if (stopping_)
        {
            throw std::runtime_error("Async writer is stopped");
        }
        if (queue_.size() >= capacity_)
        {
            throw std::runtime_error("Async writer queue is full");
        }
        operation = std::make_shared<AsyncWriteOperation>(
            nextRequestId_++, std::move(chunk));
        queue_.push_back(operation);
    }
    queueChanged_.notify_one();
    return AsyncWriteHandle(std::move(operation));
}

void AsyncChunkWriter::waitForCapacity()
{
    std::unique_lock lock(queueMutex_);
    queueChanged_.wait(lock, [this]() {
        return stopping_ || queue_.size() < capacity_;
    });
    if (stopping_)
    {
        throw std::runtime_error("Async writer is stopped");
    }
}

bool AsyncChunkWriter::cancel(const AsyncWriteHandle& handle) noexcept
{
    if (!handle.operation_) return false;
    std::lock_guard queueLock(queueMutex_);
    const auto found = std::find(queue_.begin(), queue_.end(), handle.operation_);
    if (found == queue_.end()) return false;

    const auto operation = *found;
    {
        std::lock_guard operationLock(operation->mutex);
        if (!operation->lifecycle.cancel()) return false;
        operation->chunk.reset();
    }
    queue_.erase(found);
    operation->changed.notify_all();
    return true;
}

void AsyncChunkWriter::shutdown(bool drain)
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
                    static_cast<void>(operation->lifecycle.cancel());
                    operation->chunk.reset();
                }
                operation->changed.notify_all();
            }
        }
    }
    queueChanged_.notify_all();
    for (std::thread& worker : workers_)
    {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

std::size_t AsyncChunkWriter::queuedCount() const noexcept
{
    std::lock_guard lock(queueMutex_);
    return queue_.size();
}

std::size_t AsyncChunkWriter::activeCount() const noexcept
{
    return activeCount_.load(std::memory_order_relaxed);
}

std::uint64_t AsyncChunkWriter::completedCount() const noexcept
{
    return completedCount_.load(std::memory_order_relaxed);
}

std::uint64_t AsyncChunkWriter::failedCount() const noexcept
{
    return failedCount_.load(std::memory_order_relaxed);
}

std::size_t AsyncChunkWriter::capacity() const noexcept
{
    return capacity_;
}

void AsyncChunkWriter::workerLoop()
{
    while (true)
    {
        std::shared_ptr<AsyncWriteOperation> operation;
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
            if (!operation->lifecycle.beginWriting())
            {
                activeCount_.fetch_sub(1, std::memory_order_relaxed);
                operation->changed.notify_all();
                continue;
            }
        }

        try
        {
            {
                std::lock_guard operationLock(operation->mutex);
                if (!operation->chunk.has_value())
                {
                    throw std::runtime_error("Async write payload is missing");
                }
            }
            {
                std::lock_guard operationLock(operation->mutex);
                static_cast<void>(manager_.store(*operation->chunk));
                static_cast<void>(operation->lifecycle.completeStored());
                operation->chunk.reset();
            }
        }
        catch (const std::exception& error)
        {
            std::lock_guard lock(operation->mutex);
            static_cast<void>(operation->lifecycle.fail(error.what()));
            operation->chunk.reset();
        }
        catch (...)
        {
            std::lock_guard lock(operation->mutex);
            static_cast<void>(operation->lifecycle.fail(
                "Unknown asynchronous storage failure"));
            operation->chunk.reset();
        }
        {
            std::lock_guard lock(operation->mutex);
            if (operation->lifecycle.state() == AsyncWriteState::stored)
            {
                completedCount_.fetch_add(1, std::memory_order_relaxed);
            }
            else if (operation->lifecycle.state() == AsyncWriteState::failed)
            {
                failedCount_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        activeCount_.fetch_sub(1, std::memory_order_relaxed);
        operation->changed.notify_all();
    }
}

} // namespace pi::storage
