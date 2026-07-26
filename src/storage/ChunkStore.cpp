#include "storage/ChunkStore.hpp"

#include "checkpoint/AtomicFileCommit.hpp"
#include "storage/StorageTiming.hpp"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace pi::storage
{
namespace
{
std::string formatBytes(std::uint64_t bytes)
{
    if (bytes >= 1024ULL * 1024 * 1024) return std::to_string(bytes / (1024ULL * 1024 * 1024)) + " GB";
    if (bytes >= 1024ULL * 1024) return std::to_string(bytes / (1024ULL * 1024)) + " MB";
    if (bytes >= 1024ULL) return std::to_string(bytes / 1024ULL) + " KB";
    return std::to_string(bytes) + " bytes";
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    if (const char* cacheMode = std::getenv("PI_STORAGE_CACHE_MODE");
        cacheMode != nullptr && std::string_view(cacheMode) == "cold")
    {
        const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (descriptor >= 0)
        {
            static_cast<void>(::posix_fadvise(
                descriptor, 0, 0, POSIX_FADV_DONTNEED));
            ::close(descriptor);
        }
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open chunk file: " + path.string());

    std::error_code error;
    const auto fileSize = std::filesystem::file_size(path, error);
    if (error)
        throw std::system_error(error, "cannot stat chunk file");
    if (fileSize > std::numeric_limits<std::size_t>::max()
        || fileSize > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max()))
        throw std::length_error("chunk file is too large to read");

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (bytes.empty()) return bytes;

    in.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("cannot read complete chunk file: " + path.string());
    return bytes;
}
}

ChunkStore::ChunkStore(const StoragePolicy& policy, StorageTiming* timing)
    : policy_(policy), timing_(timing)
{
    policy_.validate();
    if (!std::filesystem::exists(policy_.directory)
        && !std::filesystem::create_directories(policy_.directory))
        throw std::system_error(errno, std::generic_category(), "create storage directory");
}

const std::filesystem::path& ChunkStore::directory() const noexcept { return policy_.directory; }
std::uint64_t ChunkStore::memoryBudget() const noexcept { return policy_.memory_budget_bytes; }
std::uint64_t ChunkStore::targetChunkSize() const noexcept { return policy_.target_chunk_size_bytes; }
CompressionAlgorithm ChunkStore::compression() const noexcept { return policy_.compression; }

std::filesystem::path ChunkStore::chunkPath(ChunkId chunkId) const
{
    return policy_.directory / ChunkMetadata::createChunkFilename(std::move(chunkId));
}

bool ChunkStore::atomicRename(const std::filesystem::path& source, const std::filesystem::path& target)
{
    try { std::filesystem::rename(source, target); return true; }
    catch (const std::filesystem::filesystem_error&) { return false; }
}

void ChunkStore::syncDirectory()
{
    const int fd = ::open(policy_.directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "open storage directory");
    const int result = ::fsync(fd);
    const int savedErrno = errno;
    ::close(fd);
    if (result != 0) throw std::system_error(savedErrno, std::generic_category(), "sync storage directory");
}

void ChunkStore::syncFile(const std::filesystem::path& path)
{
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "open chunk file");
    const int result = ::fsync(fd);
    const int savedErrno = errno;
    ::close(fd);
    if (result != 0) throw std::system_error(savedErrno, std::generic_category(), "sync chunk file");
}

std::filesystem::path ChunkStore::createTemporaryFile(const std::filesystem::path& target) const
{
    const auto tempDir = policy_.directory / ".tmp";
    std::filesystem::create_directories(tempDir);
    return tempDir / (target.filename().string() + ".tmp." + std::to_string(::getpid()));
}

void ChunkStore::removeTemporaryFile(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

std::vector<std::filesystem::path> ChunkStore::listTemporaryFiles() const
{
    std::vector<std::filesystem::path> result;
    const auto dir = policy_.directory / ".tmp";
    if (!std::filesystem::is_directory(dir)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        if (entry.is_regular_file()) result.push_back(entry.path());
    std::sort(result.begin(), result.end());
    return result;
}

void ChunkStore::cleanupTemporaryFiles()
{
    for (const auto& path : listTemporaryFiles()) removeTemporaryFile(path);
}

ChunkId ChunkStore::store(const Chunk& chunk)
{
    chunk.metadata.validate();
    if (chunk.metadata.compression != policy_.compression)
        throw std::invalid_argument("chunk compression does not match storage policy");

    const auto encoded = ChunkCodec::encode(chunk, timing_);
    const ChunkId id = chunk.metadata.identity.deterministicFilename();
    const auto target = chunkPath(id);
    const auto temp = createTemporaryFile(target);
    try
    {
        const auto fileWriteStarted = std::chrono::steady_clock::now();
        std::ofstream out(temp, std::ios::binary);
        if (!out) throw std::runtime_error("cannot create temporary chunk");
        out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
        out.close();
        if (!out) throw std::runtime_error("cannot write temporary chunk");
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeFileWriteNs,
                std::chrono::steady_clock::now() - fileWriteStarted);
        const auto fileSyncStarted = std::chrono::steady_clock::now();
        syncFile(temp);
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeFileSyncNs,
                std::chrono::steady_clock::now() - fileSyncStarted);
        const auto renameStarted = std::chrono::steady_clock::now();
        if (!atomicRename(temp, target)) throw std::runtime_error("cannot publish chunk atomically");
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeRenameNs,
                std::chrono::steady_clock::now() - renameStarted);
        const auto directorySyncStarted = std::chrono::steady_clock::now();
        syncDirectory();
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->storeDirectorySyncNs,
                std::chrono::steady_clock::now() - directorySyncStarted);
    }
    catch (...)
    {
        removeTemporaryFile(temp);
        throw;
    }
    return id;
}

std::optional<Chunk> ChunkStore::load(ChunkId chunkId) const
{
    const auto path = chunkPath(std::move(chunkId));
    if (!std::filesystem::is_regular_file(path)) return std::nullopt;
    const auto readStarted = std::chrono::steady_clock::now();
    const auto bytes = readFile(path);
    if (timing_ != nullptr)
        StorageTiming::add(
            timing_->loadFileReadNs,
            std::chrono::steady_clock::now() - readStarted);
    return ChunkCodec::decode(bytes, timing_);
}

bool ChunkStore::contains(ChunkId chunkId) const
{
    return std::filesystem::is_regular_file(chunkPath(std::move(chunkId)));
}

bool ChunkStore::remove(ChunkId chunkId)
{
    std::error_code ec;
    const bool removed = std::filesystem::remove(chunkPath(std::move(chunkId)), ec);
    if (removed) syncDirectory();
    return removed;
}

std::optional<std::uint64_t> ChunkStore::fileSize(ChunkId chunkId) const
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(chunkPath(std::move(chunkId)), ec);
    return ec ? std::nullopt : std::optional<std::uint64_t>(size);
}

std::vector<std::pair<std::filesystem::path, std::uint64_t>> ChunkStore::listAllFiles() const
{
    std::vector<std::pair<std::filesystem::path, std::uint64_t>> result;
    for (const auto& entry : std::filesystem::directory_iterator(policy_.directory))
        if (entry.is_regular_file() && entry.path().extension() == ".chunk")
            result.emplace_back(entry.path(), entry.file_size());
    std::sort(result.begin(), result.end());
    return result;
}

bool ChunkStore::verifyChunkIntegrity(ChunkId chunkId) const
{
    const auto expectedId = chunkId;
    const auto path = chunkPath(chunkId);
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("chunk not found");
    try
    {
        const auto readStarted = std::chrono::steady_clock::now();
        const auto bytes = readFile(path);
        if (timing_ != nullptr)
            StorageTiming::add(
                timing_->loadFileReadNs,
                std::chrono::steady_clock::now() - readStarted);
        const Chunk decoded = ChunkCodec::decode(bytes, timing_);
        if (decoded.metadata.identity.deterministicFilename() != expectedId)
            throw std::runtime_error("chunk identity does not match filename");
        if (decoded.metadata.compression != policy_.compression)
            throw std::runtime_error("chunk compression does not match storage policy");
        return true;
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(std::string("chunk integrity check failed: ") + error.what());
    }
}

std::optional<Chunk> ChunkStore::reloadAndVerify(ChunkId chunkId) const
{
    const auto expectedId = chunkId;
    const auto path = chunkPath(std::move(chunkId));
    if (!std::filesystem::is_regular_file(path)) return std::nullopt;

    const auto readStarted = std::chrono::steady_clock::now();
    const auto bytes = readFile(path);
    if (timing_ != nullptr)
        StorageTiming::add(
            timing_->loadFileReadNs,
            std::chrono::steady_clock::now() - readStarted);

    try
    {
        auto decoded = ChunkCodec::decode(bytes, timing_);
        if (decoded.metadata.identity.deterministicFilename() != expectedId)
            throw std::runtime_error("chunk identity does not match filename");
        if (decoded.metadata.compression != policy_.compression)
            throw std::runtime_error("chunk compression does not match storage policy");
        return decoded;
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(
            std::string("chunk integrity check failed: ") + error.what());
    }
}

std::vector<std::pair<ChunkId, bool>> ChunkStore::verifyAllChunks() const
{
    std::vector<std::pair<ChunkId, bool>> result;
    for (const auto& [path, size] : listAllFiles())
    {
        bool valid = false;
        try { valid = verifyChunkIntegrity(path.filename().string()); } catch (...) {}
        result.emplace_back(path.filename().string(), valid);
    }
    return result;
}

std::size_t ChunkStore::removeCorruptedFiles() const
{
    std::size_t count = 0;
    for (const auto& [id, valid] : verifyAllChunks())
        if (!valid && const_cast<ChunkStore*>(this)->remove(id)) ++count;
    return count;
}

MemoryBudget ChunkStore::queryBudget(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
    const std::vector<std::pair<ChunkId, std::uint64_t>>& storedChunks) const
{
    MemoryBudget result;
    result.setBudget(policy_.memory_budget_bytes);
    result.setTargetChunkSize(policy_.target_chunk_size_bytes);
    for (const auto& [id, bytes] : residentChunks) { (void)id; result.addResidentBytes(bytes); }
    for (const auto& [id, bytes] : storedChunks) { (void)id; result.addStorageBytes(bytes); }
    return result;
}

std::vector<EvictionCandidate> ChunkStore::getEvictionCandidates(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
    const std::unordered_map<ChunkId, double>& mergeDistanceMap,
    std::uint64_t budget) const
{
    if (budget == 0) throw std::invalid_argument("eviction budget must be greater than zero");
    std::vector<EvictionCandidate> result;
    std::set<ChunkId> seen;
    for (const auto& [id, bytes] : residentChunks)
    {
        if (!seen.insert(id).second)
            throw std::invalid_argument("duplicate resident chunk id");
        const double distance = mergeDistanceMap.contains(id) ? mergeDistanceMap.at(id) : 0.0;
        if (!std::isfinite(distance) || distance < 0.0)
            throw std::invalid_argument("merge distance must be finite and non-negative");
        if (distance == 0.0 || bytes == 0) continue;
        result.push_back({id, bytes, fileSize(id).value_or(bytes), distance, distance, "not_immediately_needed"});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.mergeDistance != right.mergeDistance)
            return left.mergeDistance > right.mergeDistance;
        if (left.uncompressedSize != right.uncompressedSize)
            return left.uncompressedSize > right.uncompressedSize;
        return left.chunkId < right.chunkId;
    });
    return result;
}

EvictionPlan ChunkStore::planEvictions(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
    const std::set<ChunkId>& residentSet,
    const std::unordered_map<ChunkId, double>& mergeDistanceMap,
    std::uint64_t neededBytes) const
{
    EvictionPlan plan;
    for (auto candidate : getEvictionCandidates(residentChunks, mergeDistanceMap, policy_.memory_budget_bytes))
        if (residentSet.contains(candidate.chunkId)) plan.candidates.push_back(candidate);
    plan.evictUntilBudgetSatisfied(neededBytes);
    plan.updateBudgetStatus(plan.evictedBytes, neededBytes);
    if (!plan.budgetSatisfied && !plan.reason.has_value())
        plan.reason = "resident candidates cannot satisfy requested eviction bytes";
    return plan;
}

double MemoryBudget::budgetUtilization() const noexcept { return budget_bytes ? 100.0 * resident_bytes / budget_bytes : 0.0; }
double MemoryBudget::storageUtilization() const noexcept { return budget_bytes ? 100.0 * storage_bytes / budget_bytes : 0.0; }
std::uint64_t MemoryBudget::availableBudget() const noexcept { return resident_bytes >= budget_bytes ? 0 : budget_bytes - resident_bytes; }
void MemoryBudget::addResidentBytes(std::uint64_t bytes) { resident_bytes = std::numeric_limits<std::uint64_t>::max() - resident_bytes < bytes ? std::numeric_limits<std::uint64_t>::max() : resident_bytes + bytes; }
void MemoryBudget::removeResidentBytes(std::uint64_t bytes) { resident_bytes = std::max(resident_bytes, bytes) - bytes; }
void MemoryBudget::addStorageBytes(std::uint64_t bytes) { storage_bytes = std::numeric_limits<std::uint64_t>::max() - storage_bytes < bytes ? std::numeric_limits<std::uint64_t>::max() : storage_bytes + bytes; }
void MemoryBudget::removeStorageBytes(std::uint64_t bytes) { storage_bytes = std::max(storage_bytes, bytes) - bytes; }
void MemoryBudget::setBudget(std::uint64_t bytes) { budget_bytes = bytes; }
void MemoryBudget::setTargetChunkSize(std::uint64_t bytes) { target_chunk_size = bytes; }
void MemoryBudget::reset() noexcept { *this = {}; }
std::string MemoryBudget::toString() const { return "MemoryBudget{resident=" + formatBytes(resident_bytes) + ", storage=" + formatBytes(storage_bytes) + "}"; }

void EvictionPlan::updateBudgetStatus(std::uint64_t current, std::uint64_t budget)
{
    budgetSatisfied = current >= budget;
    if (budgetSatisfied) reason.reset();
    else reason = "eviction plan did not free enough resident bytes";
}
void EvictionPlan::evictUntilBudgetSatisfied(std::uint64_t neededBytes)
{
    evictedBytes = evictedStoredBytes = 0;
    std::vector<EvictionCandidate> selected;
    for (const auto& candidate : candidates)
    {
        if (evictedBytes >= neededBytes) break;
        selected.push_back(candidate);
        evictedBytes = std::numeric_limits<std::uint64_t>::max() - evictedBytes < candidate.uncompressedSize
            ? std::numeric_limits<std::uint64_t>::max() : evictedBytes + candidate.uncompressedSize;
        evictedStoredBytes = std::numeric_limits<std::uint64_t>::max() - evictedStoredBytes < candidate.storedSize
            ? std::numeric_limits<std::uint64_t>::max() : evictedStoredBytes + candidate.storedSize;
    }
    candidates = std::move(selected);
}
std::string EvictionPlan::toString() const { return "EvictionPlan{candidates=" + std::to_string(candidates.size()) + "}"; }

} // namespace pi::storage
