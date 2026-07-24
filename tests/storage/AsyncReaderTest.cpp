#include "bigint/GMPInteger.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "storage/AsyncReader.hpp"
#include "storage/ChunkCodec.hpp"
#include "storage/StorageManager.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto directory = std::filesystem::temp_directory_path()
        / "givemepi-pr0027-async-reader-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    StoragePolicy policy;
    policy.directory = directory;
    StorageManager manager(policy);

    const auto computation = ComputationIdentity::fromPrecisionPlan(
        PrecisionPolicy::create(1000));
    const auto identity = ChunkIdentity::create(
        computation,
        BlockLocation::create(0, 4, 0, computation));
    pi::bigint::GMPInteger p(12), q(5), t(7);
    const Chunk chunk{
        ChunkCodec::createMetadata(identity, p, q, t), p, q, t};
    static_cast<void>(manager.store(chunk));

    AsyncChunkReader reader(manager, 4, 4);
    auto handle = reader.loadAsync(identity);
    handle.wait();
    assert(handle.valid());
    assert(handle.state() == AsyncReadState::loaded);
    const auto loaded = handle.takeChunk();
    assert(loaded.has_value());
    assert(loaded->p.compare(p) == 0);
    assert(loaded->q.compare(q) == 0);
    assert(loaded->t.compare(t) == 0);
    assert(reader.queuedCount() == 0);
    assert(reader.activeCount() == 0);
    assert(reader.completedCount() == 1);
    assert(reader.failedCount() == 0);

    const auto missingIdentity = ChunkIdentity::create(
        computation,
        BlockLocation::create(4, 8, 0, computation));
    auto missing = reader.loadAsync(missingIdentity);
    missing.wait();
    assert(missing.state() == AsyncReadState::failed);
    assert(missing.error().has_value());
    assert(!missing.takeChunk().has_value());
    assert(reader.completedCount() == 1);
    assert(reader.failedCount() == 1);

    reader.shutdown();
    bool rejected = false;
    try
    {
        static_cast<void>(reader.loadAsync(identity));
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);
    std::filesystem::remove_all(directory, ec);
    std::cout << "AsyncReader OK\n";
    return 0;
}
