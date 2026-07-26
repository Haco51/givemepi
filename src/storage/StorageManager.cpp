#include "storage/StorageManager.hpp"
#include "storage/StorageTiming.hpp"

#include <chrono>
#include <stdexcept>

namespace pi::storage
{

std::filesystem::path StorageManager::makeIndexPath(const StoragePolicy& policy)
{
    return policy.directory / "chunk-index-v1.bin";
}

StorageManager::StorageManager(const StoragePolicy& policy, StorageTiming* timing)
    : policy_(policy)
    , store_(policy_, timing)
    , index_(ChunkIndex::create())
    , indexPath_(makeIndexPath(policy_))
    , timing_(timing)
{
    if (std::filesystem::exists(indexPath_))
        index_ = ChunkIndex::load(indexPath_);
}

void StorageManager::publishIndex(const ChunkIndex& next)
{
    next.save(indexPath_);
    index_ = next;
}

ChunkId StorageManager::store(const Chunk& chunk)
{
    const auto& identity = chunk.metadata.identity;
    const ChunkId id = identity.deterministicFilename();
    {
        std::lock_guard lock(indexMutex_);
        if (index_.contains(identity) || !inFlightStores_.insert(id).second)
            throw std::invalid_argument(
                "duplicate chunk identity in storage manager");
    }

    try
    {
        static_cast<void>(store_.store(chunk));
        const auto indexPublishStarted = std::chrono::steady_clock::now();
        std::lock_guard lock(indexMutex_);
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeIndexMutexWaitNs,
                std::chrono::steady_clock::now() - indexPublishStarted);
        const auto indexCommitStarted = std::chrono::steady_clock::now();
        ChunkIndex next = index_;
        next.add(chunk.metadata);
        publishIndex(next);
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeIndexCommitNs,
                std::chrono::steady_clock::now() - indexCommitStarted);
        inFlightStores_.erase(id);
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeIndexPublishNs,
                std::chrono::steady_clock::now() - indexPublishStarted);
    }
    catch (...)
    {
        {
            std::lock_guard lock(indexMutex_);
            try
            {
                static_cast<void>(store_.remove(id));
            }
            catch (...)
            {
                // Preserve the original store/publication failure.
            }
            inFlightStores_.erase(id);
        }
        throw;
    }
    return id;
}

std::optional<Chunk> StorageManager::load(const ChunkIdentity& identity) const
{
    ChunkIndexEntry entry;
    {
        std::lock_guard lock(indexMutex_);
        if (!index_.contains(identity)) return std::nullopt;
        entry = index_.at(identity);
    }
    const auto loaded = store_.reloadAndVerify(entry.storageFile);
    if (!loaded.has_value()) return std::nullopt;
    if (loaded->metadata.identity != identity)
        throw std::runtime_error("chunk index identity does not match stored chunk");
    if (loaded->metadata.storedSize != entry.storedSize
        || loaded->metadata.checksumValue != entry.checksumValue)
        throw std::runtime_error("chunk index metadata does not match stored chunk");
    return loaded;
}

bool StorageManager::contains(const ChunkIdentity& identity) const noexcept
{
    std::optional<ChunkId> storageFile;
    {
        std::lock_guard lock(indexMutex_);
        if (index_.contains(identity))
            storageFile = index_.at(identity).storageFile;
    }
    return storageFile.has_value() && store_.contains(*storageFile);
}

bool StorageManager::remove(const ChunkIdentity& identity)
{
    std::lock_guard lock(indexMutex_);
    if (!index_.contains(identity)) return false;
    const auto id = index_.at(identity).storageFile;
    if (!store_.remove(id)) return false;
    ChunkIndex next = index_;
    next.remove(identity);
    publishIndex(next);
    return true;
}

StorageSnapshot StorageManager::snapshot(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks
) const
{
    std::lock_guard lock(indexMutex_);
    StorageSnapshot snapshot;
    snapshot.indexedChunks = index_.size();
    snapshot.storedBytes = index_.storedBytes();
    snapshot.memoryBudgetBytes = policy_.memory_budget_bytes;
    snapshot.entries = index_.entries();
    for (const auto& [id, bytes] : residentChunks) { (void)id; snapshot.residentBytes += bytes; }
    return snapshot;
}

EvictionPlan StorageManager::planEvictions(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
    const std::set<ChunkId>& residentSet,
    const std::unordered_map<ChunkId, double>& mergeDistanceMap,
    std::uint64_t neededBytes
) const
{
    std::lock_guard lock(indexMutex_);
    return store_.planEvictions(residentChunks, residentSet, mergeDistanceMap, neededBytes);
}

} // namespace pi::storage
