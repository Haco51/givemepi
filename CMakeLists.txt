cmake_minimum_required(VERSION 3.20)

project(
    PiEngine
    VERSION 0.1.0
    DESCRIPTION "High Performance Pi Calculator"
    LANGUAGES CXX
)

enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler Options
# ---------------------------------------------------------------------------

if(MSVC)

    add_compile_options(/W4)

else()

    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        add_compile_options(
            -march=native
        )
    endif()

endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

option(ENABLE_BENCHMARKS "Build standalone benchmark executables" OFF)

find_library(LZ4_LIBRARY NAMES lz4 REQUIRED)
link_libraries(${LZ4_LIBRARY})


if(ENABLE_ASAN)

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=address
    )

endif()


if(ENABLE_BENCHMARKS)

    add_executable(out-of-core-merge-benchmark
        benchmarks/storage/OutOfCoreMergeBenchmark.cpp
        src/storage/StorageMergeCoordinator.cpp
        src/storage/AsyncWriter.cpp
        src/storage/AsyncReader.cpp
        src/storage/BinaryNodeChunkAdapter.cpp
        src/storage/NodeLifecycle.cpp
        src/storage/StorageManager.cpp
        src/storage/ChunkStore.cpp
        src/storage/ChunkIndex.cpp
        src/storage/ChunkCodec.cpp
        src/storage/CompressionCodec.cpp
        src/storage/StoragePolicy.cpp
        src/storage/ChunkTypes.cpp
        src/progress/ProgressTracker.cpp
        src/progress/ProgressSnapshot.cpp
        src/progress/ProgressState.cpp
        src/checkpoint/AtomicFileCommit.cpp
        src/checkpoint/CRC32C.cpp
        src/checkpoint/GMPSerialization.cpp
        src/checkpoint/CheckpointTypes.cpp
        src/chudnovsky/PrecisionPolicy.cpp
        src/binary/BinaryNode.cpp
        src/binary/BinarySplitter.cpp
        src/bigint/GMPInteger.cpp
        src/scheduler/Task.cpp
        src/scheduler/TaskHandle.cpp
        src/scheduler/ThreadPool.cpp
        src/scheduler/LockFreeQueue.cpp
        src/scheduler/WorkStealingQueue.cpp
        src/scheduler/Worker.cpp
        src/scheduler/Scheduler.cpp
        src/scheduler/ReferenceQueue.cpp
    )

    target_include_directories(out-of-core-merge-benchmark PRIVATE include)
    target_link_libraries(out-of-core-merge-benchmark PRIVATE gmp Threads::Threads)

    add_executable(storage-throughput-benchmark
        benchmarks/storage/StorageThroughputBenchmark.cpp
        src/storage/StorageManager.cpp
        src/storage/ChunkStore.cpp
        src/storage/ChunkIndex.cpp
        src/storage/ChunkCodec.cpp
        src/storage/CompressionCodec.cpp
        src/storage/StoragePolicy.cpp
        src/storage/ChunkTypes.cpp
        src/checkpoint/AtomicFileCommit.cpp
        src/checkpoint/CRC32C.cpp
        src/checkpoint/GMPSerialization.cpp
        src/checkpoint/CheckpointTypes.cpp
        src/chudnovsky/PrecisionPolicy.cpp
        src/bigint/GMPInteger.cpp
    )

    target_include_directories(storage-throughput-benchmark PRIVATE include)
    target_link_libraries(storage-throughput-benchmark PRIVATE gmp)

    add_executable(chudnovsky-benchmark
        benchmarks/chudnovsky/ChudnovskyBenchmark.cpp

        src/chudnovsky/PrecisionPolicy.cpp
        src/chudnovsky/ChudnovskyCalculator.cpp
        src/progress/ProgressTracker.cpp
        src/progress/ProgressSnapshot.cpp
        src/progress/ProgressState.cpp

        src/binary/BinaryNode.cpp
        src/binary/BinarySplitter.cpp
        src/bigint/GMPInteger.cpp

        src/scheduler/Task.cpp
        src/scheduler/TaskHandle.cpp
        src/scheduler/ThreadPool.cpp
        src/scheduler/LockFreeQueue.cpp
        src/scheduler/WorkStealingQueue.cpp
        src/scheduler/Worker.cpp
        src/scheduler/Scheduler.cpp
        src/scheduler/ReferenceQueue.cpp
    )

    target_include_directories(chudnovsky-benchmark
        PRIVATE
            include
    )

    target_link_libraries(chudnovsky-benchmark
        PRIVATE
            gmp
            Threads::Threads
    )

endif()


if(ENABLE_UBSAN)

    add_compile_options(
        -fsanitize=undefined
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=undefined
    )

endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

set(PI_ENGINE_SOURCES
    src/main.cpp

    src/config/Config.cpp
    src/config/ConfigLoader.cpp
    src/config/CommandLine.cpp
    src/storage/StoragePolicy.cpp
    src/storage/BinaryNodeChunkAdapter.cpp
    src/storage/NodeLifecycle.cpp
    src/storage/StorageMergeCoordinator.cpp
    src/storage/AsyncWriter.cpp
    src/storage/AsyncReader.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/ChunkTypes.cpp

    src/scheduler/ThreadPool.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp

    src/platform/CPUFeatures.cpp
    src/platform/CPUInfo.cpp

    src/memory/Arena.cpp
    src/memory/MemoryPool.cpp
    src/memory/Alignment.cpp
    src/memory/ScratchBuffer.cpp

    src/bigint/GMPInteger.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp

    src/chudnovsky/PrecisionPolicy.cpp
    src/chudnovsky/ChudnovskyCalculator.cpp

    src/checkpoint/CheckpointTypes.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/CheckpointResumePlanner.cpp

    src/progress/ProgressState.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressMetrics.cpp
    src/progress/ResumeProgress.cpp
    src/progress/ProgressReporter.cpp
    src/progress/TextProgressReporter.cpp
    src/progress/JsonProgressReporter.cpp
    src/progress/ProgressReportingRunner.cpp

    src/verification/FinalVerification.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/SHA256.cpp
    src/verification/KnownDigitsVerifier.cpp
    src/verification/BBPHexDigit.cpp
    src/verification/DecimalHexDigit.cpp
    src/verification/BBPSamplePolicy.cpp
    src/verification/FinalVerifier.cpp
    src/verification/VerificationManifest.cpp
)

set(FINAL_VERIFICATION_INTEGRATION_SOURCES
    src/chudnovsky/PrecisionPolicy.cpp
    src/chudnovsky/ChudnovskyCalculator.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
    src/verification/FinalVerification.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/SHA256.cpp
    src/verification/KnownDigitsVerifier.cpp
    src/verification/BBPHexDigit.cpp
    src/verification/DecimalHexDigit.cpp
    src/verification/BBPSamplePolicy.cpp
    src/verification/FinalVerifier.cpp
    src/verification/VerificationManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------

add_executable(pi-engine
    ${PI_ENGINE_SOURCES}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

add_executable(final-verification-test
    tests/verification/FinalVerificationTest.cpp
    src/verification/FinalVerification.cpp
)

add_executable(canonical-decimal-test
    tests/verification/CanonicalDecimalTest.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
)

add_test(
    NAME CanonicalDecimalTest
    COMMAND canonical-decimal-test
)

target_include_directories(canonical-decimal-test
    PRIVATE
        include
)

add_executable(final-output-inspector-test
    tests/verification/FinalOutputInspectorTest.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
)

add_test(
    NAME FinalOutputInspectorTest
    COMMAND final-output-inspector-test
)

target_include_directories(final-output-inspector-test
    PRIVATE
        include
)

add_executable(sha256-test
    tests/verification/SHA256Test.cpp
    src/verification/SHA256.cpp
)

add_test(
    NAME SHA256Test
    COMMAND sha256-test
)

target_include_directories(sha256-test
    PRIVATE
        include
)

add_executable(known-digits-verifier-test
    tests/verification/KnownDigitsVerifierTest.cpp
    src/verification/KnownDigitsVerifier.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
)

add_test(
    NAME KnownDigitsVerifierTest
    COMMAND known-digits-verifier-test
)

target_include_directories(known-digits-verifier-test
    PRIVATE
        include
)

add_executable(bbp-hex-digit-test
    tests/verification/BBPHexDigitTest.cpp
    src/verification/BBPHexDigit.cpp
)

add_test(NAME BBPHexDigitTest COMMAND bbp-hex-digit-test)

target_include_directories(bbp-hex-digit-test PRIVATE include)

add_executable(decimal-hex-digit-test
    tests/verification/DecimalHexDigitTest.cpp
    src/verification/DecimalHexDigit.cpp
    src/verification/BBPHexDigit.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME DecimalHexDigitTest COMMAND decimal-hex-digit-test)

target_include_directories(decimal-hex-digit-test PRIVATE include)
target_link_libraries(decimal-hex-digit-test PRIVATE gmp)

add_executable(bbp-sample-policy-test
    tests/verification/BBPSamplePolicyTest.cpp
    src/verification/BBPSamplePolicy.cpp
    src/verification/BBPHexDigit.cpp
)

add_test(NAME BBPSamplePolicyTest COMMAND bbp-sample-policy-test)

target_include_directories(bbp-sample-policy-test PRIVATE include)

add_executable(final-verifier-test
    tests/verification/FinalVerifierTest.cpp
    src/verification/FinalVerifier.cpp
    src/verification/BBPSamplePolicy.cpp
    src/verification/DecimalHexDigit.cpp
    src/verification/BBPHexDigit.cpp
    src/verification/KnownDigitsVerifier.cpp
    src/verification/SHA256.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME FinalVerifierTest COMMAND final-verifier-test)

target_include_directories(final-verifier-test PRIVATE include)
target_link_libraries(final-verifier-test PRIVATE gmp)

add_executable(verification-manifest-test
    tests/verification/VerificationManifestTest.cpp
    src/verification/VerificationManifest.cpp
    src/verification/FinalVerifier.cpp
    src/verification/BBPSamplePolicy.cpp
    src/verification/DecimalHexDigit.cpp
    src/verification/BBPHexDigit.cpp
    src/verification/KnownDigitsVerifier.cpp
    src/verification/SHA256.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME VerificationManifestTest COMMAND verification-manifest-test)

target_include_directories(verification-manifest-test PRIVATE include)
target_link_libraries(verification-manifest-test PRIVATE gmp)

add_executable(final-verification-integration-test
    tests/verification/FinalVerificationIntegrationTest.cpp
    ${FINAL_VERIFICATION_INTEGRATION_SOURCES}
)

add_test(
    NAME FinalVerificationIntegrationTest
    COMMAND final-verification-integration-test
)

target_include_directories(final-verification-integration-test
    PRIVATE
        include
        third_party/tomlplusplus/include
)
target_compile_definitions(final-verification-integration-test
    PRIVATE TOML_HEADER_ONLY=1
)
target_link_libraries(final-verification-integration-test
    PRIVATE gmp Threads::Threads
)

add_executable(large-output-streaming-test
    tests/verification/LargeOutputStreamingTest.cpp
    src/verification/FinalOutputInspector.cpp
    src/verification/CanonicalDecimal.cpp
    src/verification/FinalVerification.cpp
    src/verification/SHA256.cpp
)

add_test(NAME LargeOutputStreamingTest COMMAND large-output-streaming-test)
set_tests_properties(LargeOutputStreamingTest PROPERTIES LABELS "large")
target_include_directories(large-output-streaming-test PRIVATE include)

if(ENABLE_BENCHMARKS)
    add_executable(final-verification-benchmark
        benchmarks/verification/FinalVerificationBenchmark.cpp
        ${FINAL_VERIFICATION_INTEGRATION_SOURCES}
    )
    target_include_directories(final-verification-benchmark
        PRIVATE include third_party/tomlplusplus/include
    )
    target_compile_definitions(final-verification-benchmark
        PRIVATE TOML_HEADER_ONLY=1
    )
    target_link_libraries(final-verification-benchmark
        PRIVATE gmp Threads::Threads
    )
endif()

add_test(
    NAME FinalVerificationTest
    COMMAND final-verification-test
)

target_include_directories(final-verification-test
    PRIVATE
        include
)

add_executable(progress-config-test
    tests/config/ProgressConfigTest.cpp
    src/config/Config.cpp
    src/config/ConfigLoader.cpp
    src/config/CommandLine.cpp
    src/storage/StoragePolicy.cpp
)

add_test(
    NAME ProgressConfigTest
    COMMAND progress-config-test
)

target_include_directories(progress-config-test
    PRIVATE
        include
        third_party/tomlplusplus/include
)

target_compile_definitions(progress-config-test
    PRIVATE
        TOML_HEADER_ONLY=1
)

add_executable(progress-state-test
    tests/progress/ProgressStateTest.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressStateTest
    COMMAND progress-state-test
)

target_include_directories(progress-state-test
    PRIVATE
        include
)

add_executable(progress-snapshot-test
    tests/progress/ProgressSnapshotTest.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressSnapshotTest
    COMMAND progress-snapshot-test
)

target_include_directories(progress-snapshot-test
    PRIVATE
        include
)

add_executable(progress-tracker-test
    tests/progress/ProgressTrackerTest.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressTrackerTest
    COMMAND progress-tracker-test
)

target_include_directories(progress-tracker-test
    PRIVATE
        include
)

target_link_libraries(progress-tracker-test
    PRIVATE
        Threads::Threads
)

add_executable(progress-metrics-test
    tests/progress/ProgressMetricsTest.cpp
    src/progress/ProgressMetrics.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressMetricsTest
    COMMAND progress-metrics-test
)

target_include_directories(progress-metrics-test
    PRIVATE
        include
)

add_executable(resume-progress-test
    tests/progress/ResumeProgressTest.cpp
    src/progress/ResumeProgress.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME ResumeProgressTest
    COMMAND resume-progress-test
)

target_include_directories(resume-progress-test
    PRIVATE
        include
)

target_link_libraries(resume-progress-test
    PRIVATE
        gmp
        Threads::Threads
)

add_executable(progress-reporter-test
    tests/progress/ProgressReporterTest.cpp
    src/progress/ProgressReporter.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressReporterTest
    COMMAND progress-reporter-test
)

target_include_directories(progress-reporter-test
    PRIVATE
        include
)

add_executable(text-progress-reporter-test
    tests/progress/TextProgressReporterTest.cpp
    src/progress/TextProgressReporter.cpp
    src/progress/ProgressReporter.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME TextProgressReporterTest
    COMMAND text-progress-reporter-test
)

target_include_directories(text-progress-reporter-test
    PRIVATE
        include
)

add_executable(json-progress-reporter-test
    tests/progress/JsonProgressReporterTest.cpp
    src/progress/JsonProgressReporter.cpp
    src/progress/ProgressReporter.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME JsonProgressReporterTest
    COMMAND json-progress-reporter-test
)

target_include_directories(json-progress-reporter-test
    PRIVATE
        include
)

add_executable(progress-reporting-runner-test
    tests/progress/ProgressReportingRunnerTest.cpp
    src/progress/ProgressReportingRunner.cpp
    src/progress/ProgressReporter.cpp
    src/progress/ProgressMetrics.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
)

add_test(
    NAME ProgressReportingRunnerTest
    COMMAND progress-reporting-runner-test
)

target_include_directories(progress-reporting-runner-test
    PRIVATE
        include
)

target_link_libraries(progress-reporting-runner-test
    PRIVATE
        Threads::Threads
)

add_executable(cpu-features-test
    tests/platform/CPUFeaturesTest.cpp

    src/platform/CPUFeatures.cpp
    src/platform/CPUInfo.cpp
)

add_executable(precision-policy-test
    tests/chudnovsky/PrecisionPolicyTest.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_executable(chudnovsky-calculator-test
    tests/chudnovsky/ChudnovskyCalculatorTest.cpp

    src/chudnovsky/PrecisionPolicy.cpp
    src/chudnovsky/ChudnovskyCalculator.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME ChudnovskyCalculatorTest
    COMMAND chudnovsky-calculator-test
)

target_include_directories(chudnovsky-calculator-test
    PRIVATE
        include
)

target_link_libraries(chudnovsky-calculator-test
    PRIVATE
        gmp
        Threads::Threads
)

add_test(
    NAME PrecisionPolicyTest
    COMMAND precision-policy-test
)

target_include_directories(precision-policy-test
    PRIVATE
        include
)

add_executable(checkpoint-types-test
    tests/checkpoint/CheckpointTypesTest.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(
    NAME CheckpointTypesTest
    COMMAND checkpoint-types-test
)

target_include_directories(checkpoint-types-test
    PRIVATE
        include
)

add_executable(chunk-types-test
    tests/storage/ChunkTypesTest.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(
    NAME ChunkTypesTest
    COMMAND chunk-types-test
)

target_include_directories(chunk-types-test
    PRIVATE
        include
)

add_executable(storage-policy-test
    tests/storage/StoragePolicyTest.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(
    NAME StoragePolicyTest
    COMMAND storage-policy-test
)

target_include_directories(storage-policy-test
    PRIVATE
        include
)

add_executable(compression-codec-test
    tests/storage/CompressionCodecTest.cpp
    src/storage/CompressionCodec.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(
    NAME CompressionCodecTest
    COMMAND compression-codec-test
)

target_include_directories(compression-codec-test
    PRIVATE
        include
)

add_executable(chunk-store-test
    tests/storage/ChunkStoreTest.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME ChunkStoreTest COMMAND chunk-store-test)

target_include_directories(chunk-store-test
    PRIVATE
        include
)

target_link_libraries(chunk-store-test PRIVATE gmp)

add_executable(storage-manager-test
    tests/storage/StorageManagerTest.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME StorageManagerTest COMMAND storage-manager-test)

target_include_directories(storage-manager-test PRIVATE include)
target_link_libraries(storage-manager-test PRIVATE gmp)

add_executable(binary-node-chunk-adapter-test
    tests/storage/BinaryNodeChunkAdapterTest.cpp
    src/storage/BinaryNodeChunkAdapter.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME BinaryNodeChunkAdapterTest
    COMMAND binary-node-chunk-adapter-test
)

target_include_directories(binary-node-chunk-adapter-test PRIVATE include)
target_link_libraries(binary-node-chunk-adapter-test PRIVATE gmp)

add_executable(node-lifecycle-test
    tests/storage/NodeLifecycleTest.cpp
    src/storage/NodeLifecycle.cpp
)

add_test(NAME NodeLifecycleTest COMMAND node-lifecycle-test)

target_include_directories(node-lifecycle-test PRIVATE include)

add_executable(storage-merge-coordinator-test
    tests/storage/StorageMergeCoordinatorTest.cpp
    src/storage/StorageMergeCoordinator.cpp
    src/storage/AsyncWriter.cpp
    src/storage/AsyncReader.cpp
    src/storage/BinaryNodeChunkAdapter.cpp
    src/storage/NodeLifecycle.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/progress/ProgressTracker.cpp
    src/progress/ProgressSnapshot.cpp
    src/progress/ProgressState.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME StorageMergeCoordinatorTest
    COMMAND storage-merge-coordinator-test
)

target_include_directories(storage-merge-coordinator-test PRIVATE include)
target_link_libraries(storage-merge-coordinator-test PRIVATE gmp Threads::Threads)

add_executable(async-writer-contract-test
    tests/storage/AsyncWriterContractTest.cpp
    src/storage/AsyncWriter.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME AsyncWriterContractTest COMMAND async-writer-contract-test)
target_include_directories(async-writer-contract-test PRIVATE include)
target_link_libraries(async-writer-contract-test PRIVATE gmp Threads::Threads)

add_executable(async-writer-test
    tests/storage/AsyncWriterTest.cpp
    src/storage/AsyncWriter.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME AsyncWriterTest COMMAND async-writer-test)
target_include_directories(async-writer-test PRIVATE include)
target_link_libraries(async-writer-test PRIVATE gmp Threads::Threads)

add_executable(async-reader-test
    tests/storage/AsyncReaderTest.cpp
    src/storage/AsyncReader.cpp
    src/storage/StorageManager.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME AsyncReaderTest COMMAND async-reader-test)
target_include_directories(async-reader-test PRIVATE include)
target_link_libraries(async-reader-test PRIVATE gmp Threads::Threads)

add_executable(eviction-planner-test
    tests/storage/EvictionPlannerTest.cpp
    src/storage/ChunkStore.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/StoragePolicy.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME EvictionPlannerTest COMMAND eviction-planner-test)
target_include_directories(eviction-planner-test PRIVATE include)
target_link_libraries(eviction-planner-test PRIVATE gmp)

add_executable(chunk-index-test
    tests/storage/ChunkIndexTest.cpp
    src/storage/ChunkIndex.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(NAME ChunkIndexTest COMMAND chunk-index-test)

target_include_directories(chunk-index-test
    PRIVATE
        include
)

add_executable(chunk-codec-test
    tests/storage/ChunkCodecTest.cpp
    src/storage/VersionedChunkIndex.cpp
    src/storage/ChunkCodec.cpp
    src/storage/CompressionCodec.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME ChunkCodecTest
    COMMAND chunk-codec-test
)

target_include_directories(chunk-codec-test
    PRIVATE
        include
)

target_link_libraries(chunk-codec-test
    PRIVATE
        gmp
)

add_executable(gmp-serialization-test
    tests/checkpoint/GMPSerializationTest.cpp
    src/checkpoint/GMPSerialization.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME GMPSerializationTest
    COMMAND gmp-serialization-test
)

target_include_directories(gmp-serialization-test
    PRIVATE
        include
)

target_link_libraries(gmp-serialization-test
    PRIVATE
        gmp
)

add_executable(checkpoint-block-codec-test
    tests/checkpoint/CheckpointBlockCodecTest.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointBlockCodecTest
    COMMAND checkpoint-block-codec-test
)

target_include_directories(checkpoint-block-codec-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-block-codec-test
    PRIVATE
        gmp
)

add_executable(crc32c-test
    tests/checkpoint/CRC32CTest.cpp
    src/checkpoint/CRC32C.cpp
)

add_test(
    NAME CRC32CTest
    COMMAND crc32c-test
)

target_include_directories(crc32c-test
    PRIVATE
        include
)

add_executable(checkpoint-store-test
    tests/checkpoint/CheckpointStoreTest.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointStoreTest
    COMMAND checkpoint-store-test
)

target_include_directories(checkpoint-store-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-store-test
    PRIVATE
        gmp
)

add_executable(checkpoint-manifest-test
    tests/checkpoint/CheckpointManifestTest.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointManifestTest
    COMMAND checkpoint-manifest-test
)

target_include_directories(checkpoint-manifest-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-manifest-test
    PRIVATE
        gmp
)

add_executable(checkpoint-validation-test
    tests/checkpoint/CheckpointValidationTest.cpp
    src/checkpoint/CheckpointValidation.cpp
)

add_test(
    NAME CheckpointValidationTest
    COMMAND checkpoint-validation-test
)

target_include_directories(checkpoint-validation-test
    PRIVATE
        include
)

add_executable(checkpoint-file-inspector-test
    tests/checkpoint/CheckpointFileInspectorTest.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointFileInspectorTest
    COMMAND checkpoint-file-inspector-test
)

target_include_directories(checkpoint-file-inspector-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-file-inspector-test
    PRIVATE
        gmp
)

add_executable(checkpoint-block-validator-test
    tests/checkpoint/CheckpointBlockValidatorTest.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointBlockValidatorTest
    COMMAND checkpoint-block-validator-test
)

target_include_directories(checkpoint-block-validator-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-block-validator-test
    PRIVATE
        gmp
)

add_executable(manifest-entry-validator-test
    tests/checkpoint/ManifestEntryValidatorTest.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME ManifestEntryValidatorTest
    COMMAND manifest-entry-validator-test
)

target_include_directories(manifest-entry-validator-test
    PRIVATE
        include
)

target_link_libraries(manifest-entry-validator-test
    PRIVATE
        gmp
)

add_executable(checkpoint-manifest-validator-test
    tests/checkpoint/CheckpointManifestValidatorTest.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
)

add_test(
    NAME CheckpointManifestValidatorTest
    COMMAND checkpoint-manifest-validator-test
)

target_include_directories(checkpoint-manifest-validator-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-manifest-validator-test
    PRIVATE
        gmp
)

add_executable(modular-pqt-verifier-test
    tests/checkpoint/ModularPQTVerifierTest.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME ModularPQTVerifierTest
    COMMAND modular-pqt-verifier-test
)

target_include_directories(modular-pqt-verifier-test
    PRIVATE
        include
)

target_link_libraries(modular-pqt-verifier-test
    PRIVATE
        gmp
        Threads::Threads
)

add_executable(checkpoint-quarantine-test
    tests/checkpoint/CheckpointQuarantineTest.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointQuarantineTest
    COMMAND checkpoint-quarantine-test
)

target_include_directories(checkpoint-quarantine-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-quarantine-test
    PRIVATE
        gmp
)

add_executable(checkpoint-resume-planner-test
    tests/checkpoint/CheckpointResumePlannerTest.cpp
    src/checkpoint/CheckpointResumePlanner.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME CheckpointResumePlannerTest
    COMMAND checkpoint-resume-planner-test
)

target_include_directories(checkpoint-resume-planner-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-resume-planner-test
    PRIVATE
        gmp
        Threads::Threads
)

add_test(
    NAME CPUFeaturesTest
    COMMAND cpu-features-test
)

target_include_directories(cpu-features-test
    PRIVATE
        include
)

target_link_libraries(cpu-features-test
    PRIVATE
        Threads::Threads
)

# ---------------------------------------------------------------------------
# Memory tests
# ---------------------------------------------------------------------------

add_executable(memory-test
    tests/memory/MemoryTest.cpp

    src/memory/Arena.cpp
    src/memory/MemoryPool.cpp
    src/memory/Alignment.cpp
    src/memory/ScratchBuffer.cpp
)

add_test(
    NAME MemoryTest
    COMMAND memory-test
)


target_include_directories(memory-test
    PRIVATE
        include
)


target_link_libraries(memory-test
    PRIVATE
        Threads::Threads
)

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------

target_include_directories(pi-engine
    PRIVATE
        include
        third_party/tomlplusplus/include
)

target_compile_definitions(pi-engine
    PRIVATE
        TOML_HEADER_ONLY=1
)

# ---------------------------------------------------------------------------
# toml++
# ---------------------------------------------------------------------------
#
# Installed by:
#
# scripts/install-tomlpp.sh
#
# Repository:
#
# third_party/tomlplusplus
#
# Header-only library.
#
# Included via:
#
# third_party/tomlplusplus/include
#
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# GMP
#
# Will be integrated in a later PR.
# ---------------------------------------------------------------------------

target_link_libraries(pi-engine
    PRIVATE
        gmp
)

# ---------------------------------------------------------------------------
# Threads
#
# Will be required once ThreadPool is implemented.
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)

target_link_libraries(pi-engine
    PRIVATE
        Threads::Threads
)

add_executable(task-test
    tests/scheduler/TaskTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
)

add_test(
    NAME TaskTest
    COMMAND task-test
)

target_include_directories(task-test
    PRIVATE
        include
)

target_link_libraries(task-test
    PRIVATE
        Threads::Threads
)

add_executable(lockfreequeue-test
    tests/scheduler/LockFreeQueueTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/LockFreeQueue.cpp
)

add_test(
    NAME LockFreeQueueTest
    COMMAND lockfreequeue-test
)

target_include_directories(lockfreequeue-test
    PRIVATE
        include
)

target_link_libraries(lockfreequeue-test
    PRIVATE
        Threads::Threads
)

add_executable(scheduler-concurrency-test
    tests/scheduler/SchedulerConcurrencyTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
)

add_test(
    NAME SchedulerConcurrencyTest
    COMMAND scheduler-concurrency-test
)

target_include_directories(scheduler-concurrency-test
    PRIVATE
        include
)

target_link_libraries(scheduler-concurrency-test
    PRIVATE
        Threads::Threads
)

add_executable(scheduler-test
    tests/scheduler/SchedulerTest.cpp
    
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME SchedulerTest
    COMMAND scheduler-test
)

add_executable(threadpool-test
    tests/scheduler/ThreadPoolTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
)

add_test(
    NAME ThreadPoolTest
    COMMAND threadpool-test
)


add_executable(gmp-test
    tests/bigint/GMPIntegerTest.cpp

    src/bigint/GMPInteger.cpp
)

add_test(
    NAME GMPIntegerTest
    COMMAND gmp-test
)

add_executable(binary-test
    tests/binary/BinarySplittingTest.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp

    src/bigint/GMPInteger.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)


add_executable(versioned-chunk-index-test
    tests/storage/VersionedChunkIndexTest.cpp
    src/storage/VersionedChunkIndex.cpp
    src/storage/ChunkTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(NAME VersionedChunkIndexTest COMMAND versioned-chunk-index-test)

target_include_directories(versioned-chunk-index-test
    PRIVATE
        include
)

target_link_libraries(versioned-chunk-index-test
    PRIVATE
        gmp
)
add_test(
    NAME BinarySplittingTest
    COMMAND binary-test
)

target_include_directories(binary-test
    PRIVATE
        include
)


target_link_libraries(binary-test
    PRIVATE
        gmp
        Threads::Threads
)


target_include_directories(gmp-test
    PRIVATE
        include
)


target_link_libraries(gmp-test
    PRIVATE
        gmp
)


add_executable(workstealing-test
    tests/scheduler/WorkStealingTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME WorkStealingTest
    COMMAND workstealing-test
)

add_executable(workstealing-behavior-test
    tests/scheduler/WorkStealingBehaviorTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
)

add_test(
    NAME WorkStealingBehaviorTest
    COMMAND workstealing-behavior-test
)

target_include_directories(workstealing-behavior-test
    PRIVATE
        include
)

target_link_libraries(workstealing-behavior-test
    PRIVATE
        Threads::Threads
)

target_include_directories(workstealing-test
    PRIVATE
        include
)


target_link_libraries(workstealing-test
    PRIVATE
        Threads::Threads
)


target_include_directories(threadpool-test
    PRIVATE
        include
)


target_link_libraries(threadpool-test
    PRIVATE
        Threads::Threads
)

target_include_directories(scheduler-test
    PRIVATE
        include
)

target_link_libraries(scheduler-test
    PRIVATE
        Threads::Threads
)
