# ROADMAP

Phase 0

[x] Project

[x] CMake

[ ] Logger

[x] Config

[x] CI Automation

----------------------

Phase 1

Platform

- CPUID
- Huge Pages
- NUMA

----------------------

Phase 2

Memory

- Arena
- Pool
- Alignment

----------------------

Phase 3

Scheduler

[x] Thread Pool
[x] Queue (Reference Implementation)
[x] Lock-Free Queue
[x] Work Stealing
[x] Shared Task State (PR-0016)
[x] Task Handle / Future / Join (PR-0016)
[x] Scheduler Lifecycle Contract (PR-0017)
[x] Scheduler Correctness Hardening (PR-0017)
[x] Drain Shutdown Semantics (PR-0017)
[x] Global / Local Queue Role Validation (PR-0017)
[x] Lock-Free Queue Correctness Hardening (PR-0017)
[x] Direct Scheduler Concurrency Tests (PR-0017)
[x] Observable Work-Stealing Behavior Test (PR-0017)

----------------------

Phase 4

Big Integer

[x] GMP Wrapper

----------------------

Phase 5

Binary Splitting

[x] Node
[x] Merge
[x] Recursive Split
[x] Parallel API Refactoring
[x] Chudnovsky Leaf Integration (PR-0018)
[x] Input Range Validation (PR-0018)
[x] Small Known-Digits Integration Test (PR-0018)
[x] Parallel Cutoff Policy (PR-0019)
[x] Parallel Execution (PR-0019)
[x] Parallel Merge (PR-0019)

----------------------

Phase 6

Chudnovsky

[x] End-to-End Formula (PR-0020)
[x] Integer Fixed-Point Finalization (PR-0020)
[x] Precision and Guard-Digit Policy (PR-0020)
[x] Digit-to-Term Conversion (PR-0020)
[x] Normalized Decimal Output (PR-0020)
[x] Production Known-Digits Integration Test (PR-0020)
[x] End-to-End Benchmark (PR-0020)

----------------------

Phase 7

Storage

[x] PR-0025 Runtime Storage Foundation
[x] PR-0026 Synchronous Out-of-Core Stabilization
[x] PR-0027 Asynchronous Storage Pipeline
[x] PR-0028 Concurrent Storage Pipeline Implementation
[ ] PR-0029 Measurement and Bottleneck Optimization

----------------------

Phase 8

Checkpoint

[x] P/Q/T Block Format (PR-0021)
[x] Versioned Block Header and Payload Metadata (PR-0021)
[x] Atomic Temporary File Commit (PR-0021)
[x] Manifest Format (PR-0021)
[x] Structural Block Validation (PR-0022)
[x] Payload Checksum Validation (PR-0022)
[x] Independent Modular P/Q/T Verification (PR-0022)
[x] Manifest Consistency Validation (PR-0022)
[x] Resume from Verified Blocks Only (PR-0022)
[x] Corrupt Block Quarantine and Recalculation (PR-0022)

----------------------

Phase 9

Progress

[x] Progress Phase Model (PR-0023)
[x] Thread-Safe Progress Tracker (PR-0023)
[x] Immutable Progress Snapshot (PR-0023)
[x] Terms, Blocks, and Merge-Level Counters (PR-0023)
[x] Speed, ETA, Memory, and Checkpoint Metrics (PR-0023)
[x] CLI Text Reporter (PR-0023)
[x] CLI JSON Reporter (PR-0023)
[x] TOML and CLI Progress Controls (PR-0023)
[x] Post-Override Configuration Validation (PR-0023)
[x] Resume Progress Reconstruction (PR-0023)

----------------------

Phase 10

Verification

[x] Canonical Final Output Inspection (PR-0024)
[x] Streaming SHA-256 and Manifest Hash Revalidation (PR-0024)
[x] Versioned Known Digits (PR-0024)
[x] Independent BBP Spot Check (PR-0024)
[x] GMP Decimal-to-Hex Cross-Check (PR-0024)
[x] Versioned Verification Manifest (PR-0024)
[x] CLI and Progress Integration (PR-0024)
[x] Bounded-Memory and Release Performance Validation (PR-0024)

----------------------

Phase 11

SIMD

----------------------

Phase 12

Assembly

----------------------

Phase 13

Benchmark

----------------------

Phase 14

Performance Tuning
