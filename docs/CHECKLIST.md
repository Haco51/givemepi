# CHECKLIST

## Foundation

[x] CMake

[ ] Logger

[x] Config

[x] CLI

[x] Project directory structure

[x] README Algorithm, Architecture Direction, and Current Status

[x] Initial scheduler module skeleton

-----------------

## Automation

[x] CTest

[x] GitHub Actions CI

[x] Automated Build

[x] Automated Test

[x] GCC Build Matrix

[x] Clang Build Matrix

[x] clang-format Check

[x] clang-tidy Static Analysis

[x] AddressSanitizer

[x] UBSan

[x] Artifact Upload

[ ] Benchmark Workflow

[ ] Release Workflow


-----------------


## Platform

[x] CPUID

[x] AVX Detect

[x] AVX2 Detect

[x] AVX512 Detect

[ ] Huge Pages

[ ] NUMA

-----------------

## Memory

[x] Arena

[x] Pool

[x] Alignment

[x] Scratch Buffer

-----------------

## Scheduler

[x] Task

[x] Worker

[x] Scheduler

[x] ThreadPool

[x] Reference Queue

[x] Lock-Free Queue

[x] Work Stealing

[x] Task Synchronization

[x] Task Handle / Future

[x] Task Join

[x] Shared Task State

[x] Task Execute Lifecycle

[x] Task Exception Capture

[x] TaskHandle Exception Inspection and Rethrow

[x] TaskHandle Validity

[x] TaskHandle Completion Wait

[x] Scheduler / ThreadPool Unified Submit Handle API

[x] Scheduler Synchronization Test Scenarios

[x] Scheduler Lifecycle State

[x] Concurrent Start / Stop Serialization

[x] Idempotent Start / Stop

[x] Scheduler Restart After Stop

[x] Submission Rejection While Stopping

[x] Drain Shutdown Semantics

[x] Accepted Outstanding Task Accounting

[x] Two-Phase Worker Stop and Join

[x] Global / Local Queue Role Contract

[x] Cross-Pool Worker Submission Routing

[x] Global Queue Capacity Rejection

[x] Lock-Free Queue Capacity Validation

[x] Lock-Free Queue Sequence / Contention Correctness

[x] Lock-Free Queue Slot Reuse Coverage

[x] Lock-Free Queue Concurrent Tests

[x] Lock-Free Queue Exactly-Once MPMC Coverage

[x] Concurrent Submit / Stop Acceptance Cutoff Test

[x] Concurrent Accepted / Rejected Execution Accounting

[x] Multi-Caller Repeated Start / Stop Test

[x] Dedicated Scheduler Concurrency Test Target

[x] Work-Stealing Behavior Tests

[x] Worker-Local Child Concentration Test

[x] Multiple Thief Thread Observation

[x] Root / Thief Thread Identity Validation

[x] Global Fallback Exclusion Test Queue

[x] Work-Stealing Load / Behavior Test Separation

[x] Scheduler Correctness Hardening (PR-0017)

-----------------

## Big Integer

[x] GMP

[x] In-Place Sign Negation

[ ] FFT

-----------------

## Binary Splitting

[x] Node

[x] Merge

[x] Recursive Split

[x] Parallel API Refactoring

[x] Chudnovsky Leaf Integration

[x] Input Range Validation

[x] Leaf and Small-Range P/Q/T Correctness

[x] Small Known-Digits Integration Test

[x] Binary Splitting Boundary and Regression Coverage

[x] Parallel Cutoff Policy

[x] Parallel Execution

[x] Parallel Merge

[x] Staged Parallel DAG

[x] Parallel Submission Fallback

[x] Worker-Context Deadlock Avoidance

[x] Observable Parallel Binary Splitting Test

-----------------

## Chudnovsky

[x] Formula

[x] Precision Control

[x] Precision and Guard-Digit Policy

[x] Overflow-Safe Digit-to-Term Conversion

[x] Estimated Binary Precision

[x] GMP Power-of-Ten Operation

[x] GMP Floor Integer Square Root

[x] GMP Floor Division

[x] End-to-End Sequential Pi Calculation

[x] End-to-End Parallel Pi Calculation

[x] Guard-Digit Rounding

[x] Normalized Decimal Output

[x] Production Known-Digits Coverage

[x] Guard-Precision Stability at 1,000 Digits

[x] Split / Finalize / Format Timing Metrics

[x] Standalone End-to-End Benchmark Target

[x] Sequential / Parallel Benchmark Result Validation

-----------------

## Storage

[x] Storage Lifecycle and Ownership Contract (PR-0025)

[x] Chunk Identity and Metadata (PR-0025)

[x] Typed Storage Policy Model (PR-0025)

[x] Versioned Chunk Format (PR-0025)

[x] Compression Codec Interface and None Codec (PR-0025)

[ ] Compression

[x] Chunk Index Contract and Durable Codec (PR-0025)

[x] ChunkCodec-backed Local ChunkStore and Reload (PR-0025)

[x] Synchronous StorageManager Boundary (PR-0025)

[x] Deterministic Memory Budget and Eviction Planner (PR-0025)

[x] Storage Configuration and Progress Telemetry Integration (PR-0025)

[x] Large Synthetic P/Q/T Round-trip and Throughput Validation (PR-0025)

[x] In-memory versus Forced Out-of-Core Merge Benchmark (PR-0026)

[x] Synchronous Out-of-Core Merge Stabilization Boundary (PR-0026)

[x] Production StorageMergeCoordinator Wiring (PR-0027)

[x] Asynchronous Storage Writer Contract (PR-0027)

[x] Bounded Asynchronous I/O Queue and Writer Runner (PR-0027)

[x] Asynchronous NodeLifecycle Completion Transition (PR-0027)

[x] Merge Wait and Backpressure (PR-0027)

[x] Asynchronous Reload Reader and Merge Integration (PR-0027)

[x] Asynchronous Storage I/O Progress Telemetry (PR-0027)

[x] Asynchronous I/O Failure and Shutdown Tests (PR-0027)

[x] Async Spill/Reload Performance Baseline (PR-0027)

[x] Concurrent I/O Execution (PR-0028)

[x] PR-0028 Functional Storage Pipeline Verification

[x] Platform Applicability Audit (PR-0028)

[x] Compression Backend Optimization and Selection (PR-0029; LZ4 space/RSS acceptance, latency trade-off documented)

[x] LZ4 Compression Backend and Durable Round-trip (PR-0028)

[ ] NUMA, Huge Pages, and Affinity Optimization (PR-0029)

[x] Separate file-read, CRC32C, and GMP decode timings (PR-0029)

[x] Cold-cache versus warm-cache reload benchmark

[x] Multi-chunk and concurrent I/O throughput benchmark (256 cells × 5 repetitions, 1,280/1,280 successful; cold/warm accepted)

[x] None versus LZ4 size/throughput comparison (5x12-cell acceptance; no latency-speedup claim)

[x] Single-chunk peak RSS instrumentation and release baseline for 100 MiB, 512 MiB, and 1 GiB workloads

[x] Single-chunk peak RSS five-sample distribution and p95 acceptance (12 cells, 60/60 successful; p95 below the recorded PR-0028 release baseline)

[x] Single-chunk five-sample p50/p95 matrix completion

[x] Large-data ASan/UBSan round-trip validation (512 MiB and 1 GiB; LeakSanitizer environment-limited)

[x] PR-0028 High-Performance Measurement Contract

[x] Independent Reader/Writer Concurrency Benchmark (64 cells × 5 repetitions, 320/320 successful; RSS is not emitted by the current benchmark)

[x] I/O, CRC32C, Codec, and GMP Decode Timing Breakdown (PR-0029)

[x] PR-0029 Final Performance Decision and Stabilization (64/64 CTest; Release and sanitizer smoke)

[x] Verified Reload Duplicate Read/Decode Elimination (PR-0029)

[x] Bounded Buffer/Data-Movement Optimization Validation (round-trip and Release baseline)

[x] Bounded Buffer/Data-Movement Repeated Speedup Acceptance (12-cell end-to-end before/after; individual stage attribution limited)

[x] CRC32C SSE4.2/fallback, pre-sized read, and positive GMP export optimization validation

[x] CRC/read/GMP Repeated Speedup Acceptance (combined end-to-end before/after; baseline stage telemetry unavailable)

[x] PR-0029 Measurement Environment and Acceptance Contract

[x] PR-0029 Workload Matrix Baseline (multi-chunk)

-----------------

## Checkpoint

[x] P/Q/T Block Format

[x] Block Magic and Format Version

[x] Range and Tree-Level Metadata

[x] P/Q/T Payload Length Metadata

[x] Checksum Type and Value Metadata

[x] Atomic Temporary File Commit

[x] Manifest

[x] Save

[x] Structural Block Validation

[x] Payload Checksum Validation

[x] Manifest Consistency Validation

[x] Resume from Verified Blocks

[x] Corrupt Block Quarantine

[x] Corrupt Block Recalculation

[x] CRC

-----------------

## Progress

[x] Progress Phase

[x] Thread-Safe Progress Tracker

[x] Immutable Progress Snapshot

[x] Total and Completed Terms

[x] Total and Completed Blocks

[x] Current Merge Level

[x] Active and Queued Tasks

[x] ETA

[x] Speed

[x] Memory

[ ] Chunk

[x] Checkpoint Bytes

[x] Last Verified Checkpoint

[x] CLI Text Reporter

[x] CLI JSON Reporter

[x] TTY and Non-TTY Output Modes

[x] Progress Enable / Disable Option

[x] Progress Interval Option

[x] Progress Format Option

[x] Post-CLI Configuration Validation

[x] Resume Progress Reconstruction

[x] Future Reporter Extension Point

-----------------

## Verification

[x] Independent Modular P/Q/T Verification

[x] Canonical Output Contract

[x] Streaming SHA-256

[x] Versioned Known Digits

[x] BBP Hexadecimal Digit Calculation

[x] GMP Decimal-to-Hex Digit Extraction

[x] Deterministic BBP Sample Policy

[x] Final Verification Orchestrator

[x] Versioned Verification Manifest

[x] Manifest Hash Revalidation

[x] CLI Verification Controls and Diagnostics

[x] Verifying Output Progress Phase

[x] 128 MiB Bounded-Memory Streaming Verification

[x] Final Verification Release Benchmark

-----------------

## SIMD

[ ] AVX2

[ ] AVX512

-----------------

## ASM

[ ] CPUID

[ ] memcpy

[ ] Prefetch

[ ] rdtsc

-----------------

## Benchmark

[ ] Single Thread

[ ] Multi Thread

[ ] Memory

[ ] Storage
