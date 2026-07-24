#include "bigint/GMPInteger.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "storage/AsyncWriter.hpp"
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
        / "givemepi-pr0027-async-writer-test";
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

    AsyncChunkWriter writer(manager, 4, 4);
    const auto handle = writer.submit(chunk);
    handle.wait();
    assert(handle.valid());
    assert(handle.state() == AsyncWriteState::stored);
    assert(handle.hasDurableCopy());
    assert(!handle.error().has_value());
    assert(manager.contains(identity));
    assert(writer.queuedCount() == 0);
    assert(writer.activeCount() == 0);
    assert(writer.completedCount() == 1);
    assert(writer.failedCount() == 0);

    // Publishing the same identity twice must fail and must not be reported
    // as another durable completion.
    auto duplicate = writer.submit(chunk);
    duplicate.wait();
    assert(duplicate.state() == AsyncWriteState::failed);
    assert(!duplicate.hasDurableCopy());
    assert(duplicate.error().has_value());
    assert(writer.completedCount() == 1);
    assert(writer.failedCount() == 1);

    writer.shutdown();
    bool rejected = false;
    try
    {
        static_cast<void>(writer.submit(chunk));
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);

    std::filesystem::remove_all(directory, ec);
    std::cout << "AsyncWriter OK\n";
    return 0;
}
