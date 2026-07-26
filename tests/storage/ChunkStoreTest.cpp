#include "storage/ChunkStore.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"
#include "bigint/GMPInteger.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main()
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto directory = std::filesystem::temp_directory_path() / "givemepi-pr0025-store-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);

    const auto identity = ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const auto location = BlockLocation::create(0, 4, 0, identity);
    const auto chunkIdentity = ChunkIdentity::create(identity, location);
    pi::bigint::GMPInteger p(12);
    pi::bigint::GMPInteger q(5);
    pi::bigint::GMPInteger t(7);
    const auto metadata = ChunkCodec::createMetadata(chunkIdentity, p, q, t);
    const Chunk chunk{metadata, p, q, t};

    StoragePolicy policy;
    policy.directory = directory;
    policy.validate();
    StorageTiming timing;
    ChunkStore store(policy, &timing);
    const auto id = store.store(chunk);

    const auto loaded = store.load(id);
    if (!store.contains(id) || !loaded.has_value()
        || loaded->metadata != metadata
        || loaded->p.compare(p) != 0 || loaded->q.compare(q) != 0
        || loaded->t.compare(t) != 0
        || !store.reloadAndVerify(id).has_value())
    {
        std::cerr << "ChunkStore round trip failed\n";
        return 1;
    }
    if (timing.storeGmpEncodeNs.load() == 0
        || timing.storeFileWriteNs.load() == 0
        || timing.loadFileReadNs.load() == 0
        || timing.loadCrcNs.load() == 0
        || timing.loadGmpDecodeNs.load() == 0)
    {
        std::cerr << "ChunkStore timing telemetry missing\n";
        return 1;
    }

    {
        std::ofstream corrupt(store.chunkPath(id), std::ios::binary | std::ios::app);
        corrupt.put('\x7f');
    }
    bool rejected = false;
    try { store.verifyChunkIntegrity(id); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected || store.removeCorruptedFiles() != 1 || store.contains(id))
    {
        std::cerr << "ChunkStore corruption handling failed\n";
        return 1;
    }

    const auto compressedDirectory = directory.string() + "-lz4";
    std::filesystem::remove_all(compressedDirectory, ec);
    StoragePolicy compressedPolicy = policy;
    compressedPolicy.directory = compressedDirectory;
    compressedPolicy.compression = CompressionAlgorithm::lz4;
    ChunkStore compressedStore(compressedPolicy);
    const auto compressedMetadata = ChunkCodec::createMetadata(
        chunkIdentity, p, q, t, CompressionAlgorithm::lz4);
    const Chunk compressedChunk{compressedMetadata, p, q, t};
    const auto compressedId = compressedStore.store(compressedChunk);
    const auto compressedLoaded = compressedStore.reloadAndVerify(compressedId);
    if (!compressedLoaded.has_value()
        || compressedLoaded->metadata.compression != CompressionAlgorithm::lz4
        || compressedLoaded->p.compare(p) != 0
        || compressedLoaded->q.compare(q) != 0
        || compressedLoaded->t.compare(t) != 0)
    {
        std::cerr << "ChunkStore LZ4 round trip failed\n";
        return 1;
    }

    std::filesystem::remove_all(directory, ec);
    std::filesystem::remove_all(compressedDirectory, ec);
    std::cout << "ChunkStore OK\n";
    return 0;
}
