#pragma once

#include "storage/ChunkIndex.hpp"
#include "storage/ChunkStore.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace pi::storage
{

struct StorageSnapshot
{
    std::size_t indexedChunks = 0;
    std::uint64_t storedBytes = 0;
    std::uint64_t residentBytes = 0;
    std::uint64_t memoryBudgetBytes = 0;
    std::vector<ChunkIndexEntry> entries;
};

class StorageManager
{
public:
    explicit StorageManager(const StoragePolicy& policy);

    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    [[nodiscard]] const StoragePolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] const std::filesystem::path& indexPath() const noexcept { return indexPath_; }

    // Synchronous store: publish the chunk first, then atomically publish its index entry.
    ChunkId store(const Chunk& chunk);
    [[nodiscard]] std::optional<Chunk> load(const ChunkIdentity& identity) const;
    [[nodiscard]] bool contains(const ChunkIdentity& identity) const noexcept;
    bool remove(const ChunkIdentity& identity);

    [[nodiscard]] StorageSnapshot snapshot(
        const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks = {}
    ) const;

    [[nodiscard]] EvictionPlan planEvictions(
        const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
        const std::set<ChunkId>& residentSet,
        const std::unordered_map<ChunkId, double>& mergeDistanceMap,
        std::uint64_t neededBytes
    ) const;

    [[nodiscard]] const ChunkIndex& index() const noexcept { return index_; }

private:
    static std::filesystem::path makeIndexPath(const StoragePolicy& policy);
    void publishIndex(const ChunkIndex& next);

    StoragePolicy policy_;
    ChunkStore store_;
    ChunkIndex index_;
    std::filesystem::path indexPath_;
    mutable std::mutex indexMutex_;
    std::set<ChunkId> inFlightStores_;
};

} // namespace pi::storage
