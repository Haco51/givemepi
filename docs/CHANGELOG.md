# Changelog

## [Unreleased]

### Added

- Added CRC/read/GMP hot-path optimizations for PR-0029: runtime-selected
  SSE4.2 CRC32C with a portable fallback, exact-size single-read chunk loading,
  and copy-free positive GMP magnitude export. Canonical bytes, checksum
  coverage, and corruption handling remain unchanged; combined end-to-end
  speedup acceptance is complete.
- Added an independent reader/writer concurrency benchmark that preloads a
  reader population, concurrently submits new writer chunks and reloads reader
  chunks, and reports throughput, queue waits, active I/O, completion counts,
  and final index cardinality.
- Completed the none-vs-LZ4 compression acceptance matrix: 60/60 Release
  round-trips passed across 100/512/1024 MiB on tmpfs and ext4. LZ4 reduced
  encoded size by 29.7% and peak RSS by about 5.6~5.8% in every cell; its
  latency is recorded as a workload-dependent trade-off rather than a claimed
  universal speedup.
- Recorded the consolidated PR-0029 change history covering storage telemetry,
  reload, bounded-buffer, compression, CRC/read/GMP, concurrency benchmark,
  stability validation, and remaining acceptance items in the canonical
  changelog, checklist, decision log, and implementation plan.
- Fixed the PR-0029 documentation layout by keeping implementation order and
  remaining acceptance work in `docs/IMPLEMENTATION_PLAN.md`, with completion
  history, decisions, and task state kept in the canonical changelog,
  decisions, and checklist documents.
- Preserved a complete PR-0028-baseline versus current PR-0029 Release raw
  population for 12 cells (100/512/1024 MiB × tmpfs/ext4 × none/LZ4), five
  repetitions per cell, at `/tmp/givemepi-pr0029-before-after-complete-before.tsv`
  and `/tmp/givemepi-pr0029-before-after-complete-after.tsv`. Comparable
  store-plus-load p50 improved by 56.0~66.1% across the cells and encoded
  bytes were unchanged. The baseline executable predates stage telemetry, so
  individual CRC/read/GMP attribution and baseline RSS comparison remain
  separate from this end-to-end effect measurement.
- Accepted the combined bounded-buffer plus CRC/read/GMP end-to-end effect:
  store-plus-load p50 improved in all 12 cells by 54.5~66.1%, and p95 improved
  in all cells by 50.9~70.5%, with 60/60 before and 60/60 after round-trips
  successful. This is a combined effect because the PR-0028 baseline has no
  stage telemetry; individual CRC/read/GMP percentage attribution is not
  claimed.

- Added a version-1 runtime chunk codec with explicit big-endian metadata,
  canonical P/Q/T signed-magnitude payloads, CRC32C coverage, checked lengths,
  deterministic round trips, and corruption/version rejection.
- Added a bounded compression codec interface and the `none` codec, including
  exact input/output size contracts and explicit rejection of unimplemented
  compression algorithms.
- Added a TOML- and CLI-independent typed `StoragePolicy` with storage
  enablement, directory, memory budget, target chunk size, compression, and
  maximum concurrent I/O settings plus shared validation and compression
  parsing.
- Added runtime `ChunkIdentity` and `ChunkMetadata` types with computation
  identity and range validation, deterministic filenames, format and checksum
  metadata, and explicit rejection of unsupported compression algorithms.
- Defined the PR-0025 runtime storage lifecycle and ownership boundary,
  including resident, spilling, stored, loading, corrupted, and removed
  states, failure preservation, durable publication, and separation from
  checkpoint resume evidence.
- Added the canonical PR-0025 `ChunkIndex` contract: deterministic identity
  ordering, duplicate and same-level range-overlap rejection, stored-size
  aggregation, CRC32C-protected serialization, atomic save, and restart load.
- Integrated `ChunkStore` with the canonical `ChunkCodec`: stored files now
  contain the versioned P/Q/T chunk format directly, without metadata
  sidecars, and reload validates identity, sizes, CRC32C, compression policy,
  and canonical GMP decoding.
- Added the synchronous `StorageManager` boundary for durable chunk publish,
  index-backed reload, restart reconstruction, removal, snapshots, and
  eviction planning.
- Added restart/integrity integration coverage for durable chunk/index
  corruption and a configurable storage throughput benchmark. On the current
  runner, a 99.66 MiB synthetic P/Q/T chunk measured 157.17 MiB/s store and
  10.47 MiB/s reload throughput.
- Hardened memory accounting with saturating byte totals and deterministic
  eviction ordering: merge-protected chunks are excluded, invalid distances
  and duplicate resident IDs are rejected, and plans report unsatisfied byte
  requirements explicitly.
- Added the version-1 final verification result model with immutable stage
  results, structured diagnostics, computation identity, stage timings,
  deterministic status aggregation, and stable machine-readable names.
- Defined the canonical in-memory final decimal as exactly `3.` plus the
  requested number of ASCII digits, with immutable ownership, a newline-free
  hash domain, and structured first-error offsets for malformed candidates.
- Added separate memory and stored-file final output inspection APIs, with
  fixed-size streaming file validation, exact single-newline record framing,
  canonical byte-range metadata, and no whole-file allocation.
- Added an incremental FIPS 180-4 SHA-256 implementation with standard-vector
  coverage, chunk-independent updates, strict finalization lifecycle, lowercase
  digest encoding, and bounded-memory file-prefix hashing.
- Added a versioned 100-digit pi reference and bounded known-digits verifier
  for canonical memory values and inspected files, including compared-prefix
  metadata and exact canonical mismatch offsets.
- Added an independent BBP hexadecimal fractional-digit calculator using
  overflow-safe modular exponentiation, a bounded fractional tail, and
  conservative inconclusive results near numerical digit boundaries.
- Added reusable GMP-backed decimal-to-hexadecimal digit extraction for memory
  and streaming file inputs, with exact integer arithmetic and conservative
  decimal-rounding intervals that reject unsafe trailing positions.
- Added a configurable deterministic BBP sampling policy with fixed early and
  quartile coverage, identity-derived positions, conservative precision bounds,
  uniqueness, stable ordering, and a schema-bounded sample-count limit.
- Added a final verification orchestrator for memory and stored outputs that
  preserves structure, known-prefix, canonical SHA-256, and per-sample BBP
  diagnostics while isolating failures and stopping math on malformed output.
- Added a canonical version-1 verification manifest with output identity,
  SHA-256, known-reference and BBP evidence, CRC32C corruption detection, and
  durable same-directory atomic replacement for fully passed results.
- Connected final verification and manifest publication to the CLI with
  configurable enablement and BBP sample count, added the version-2
  `verifying_output` progress phase, and kept progress reporting active through
  writing, verification, failure, and completion.
- Added a 1,000-digit calculation-to-manifest integration regression and an
  opt-in Release benchmark that measures eight-sample final verification
  overhead independently at 1,000, 10,000, and 100,000 decimal digits.
- Added manifest-hash revalidation for stored outputs, explicit BBP
  inconclusive and mutation coverage, reporter verification-phase assertions,
  and a 128 MiB bounded-memory streaming inspection and SHA-256 regression.

### Fixed

- Corrected known-digit verification to compare against the rounded prefix at
  the requested precision instead of incorrectly treating every output as a
  truncated decimal prefix.
- Treated inapplicable skipped checks as neutral when other required final
  verification stages pass, allowing valid very-small outputs to complete
  while preserving all-skipped reports as skipped.
- Added the version-1 progress lifecycle contract with stable phase names,
  derived terminal states, explicit legal transitions, and terminal-state
  immutability.
- Added an immutable, presentation-independent progress snapshot containing
  authoritative work, scheduler, timing, memory, checkpoint, and failure data
  with construction-time invariant validation.
- Added a thread-safe progress tracker with bounded atomic work counters,
  low-frequency synchronized metadata, legal terminal transitions, frozen
  terminal elapsed time, and concurrent immutable snapshot generation.
- Added policy-driven derived progress metrics for completion ratio, lifetime
  term and checkpoint throughput, and conservative optional ETA without
  persisting estimates as authoritative state.
- Added all-or-nothing resume progress restoration from PR-0022 accepted block
  metadata, including overlap and bounds protection, exact file-byte recovery,
  and exclusion of rejected or temporary checkpoint state.
- Added a CLI-independent progress reporter interface that consumes immutable
  snapshots and matching derived metrics while leaving scheduling, sampling,
  and failure isolation to the reporting runner.
- Added a human-readable text progress reporter with single-line TTY refresh,
  newline-delimited UTC log records, terminal-state line completion, stable
  units, optional ETA handling, and control-character sanitization.
- Added a versioned JSON Lines progress reporter with stable field types,
  explicit nulls for unavailable metrics, portable UTC timestamps, complete
  raw snapshot fields, and strict string and non-finite-number handling.
- Added a dedicated progress reporting runner with explicit start, stop, and
  join lifecycle, latest-state sampling without backlog, terminal flush, and
  containment of slow or throwing reporters away from computation workers.
- Added typed TOML and CLI progress controls for enablement, reporting interval,
  and text or JSON format with shared post-override validation and the fixed
  defaults-to-file-to-command-line precedence order.
- Connected the end-to-end Chudnovsky API to optional progress lifecycle and
  completed-term updates, and wired the CLI to configure a dedicated reporter,
  run the scheduler-backed calculation, emit a terminal snapshot, and write
  the resulting decimal output file.

## [0.12.0] - PR-0021

### Added

- Added versioned computation identity and block-location value types while
  keeping execution provenance outside mathematical compatibility.
- Added canonical signed-magnitude GMP serialization with minimal big-endian
  magnitudes and strict rejection of non-canonical forms.
- Added a portable version-1 P/Q/T checkpoint codec with explicit big-endian
  fields, checked length arithmetic, and CRC32C coverage.
- Added deterministic synchronous checkpoint storage with unique
  same-directory temp files, durable atomic replacement, and fault-injection
  coverage proving pre-rename failures preserve the previous final file.
- Added a canonical versioned manifest with sorted entries, CRC32C protection,
  deterministic naming, and atomic replacement.
- Added in-memory and filesystem round-trip tests for zero, signed, and
  10,000-digit GMP values plus malformed and corrupted input coverage.

### Changed

- Restricted the checkpoint ignore rule to the repository-root runtime
  directory so checkpoint source and test directories remain trackable.

### Documentation

- Reworked the README to describe PiEngine's current implementation status,
  Chudnovsky and P/Q/T Binary Splitting direction, GMP arithmetic boundary,
  scheduler model, checkpoint integrity plan, out-of-core goal, verification
  strategy, build workflow, and explicit non-goals.
- Distinguished implemented scheduler and arithmetic foundations from planned
  Chudnovsky leaf, parallel Binary Splitting, checkpoint, progress, and final
  output stages.
- Reordered the approved plan to complete guarded integer fixed-point
  Chudnovsky finalization and end-to-end benchmarking before checkpoint format,
  integrity, and progress work.
- Defined the PR-0021 canonical P/Q/T block, mathematical identity, CRC32C,
  atomic commit, deterministic manifest, filesystem boundary, and ordered
  implementation plan.

## [0.11.0] - PR-0020

### Added

- Added a deterministic integer-only Chudnovsky precision policy covering
  requested digits, guard digits, working precision, term count, and estimated
  binary precision.
- Added overflow-safe fixed-point digit-to-term conversion with a conservative
  digits-per-term lower bound and one safety term, including billion-digit and
  invalid-boundary coverage.
- Added GMPInteger power-of-ten, non-negative floor square root, and signed
  floor division operations for integer fixed-point pi finalization, including
  domain-error and value-preservation coverage.
- Added a CLI-independent end-to-end Chudnovsky calculation API that returns
  its precision identity with a normalized, guard-rounded decimal pi string.
- Added sequential, stopped-scheduler fallback, and staged-parallel calculation
  coverage with exact rounded known values from 1 through 100 decimal places.
- Added per-calculation split, fixed-point finalization, decimal formatting, and
  total timing metrics.
- Added an opt-in standalone Release benchmark covering sequential and 4/8
  worker end-to-end calculations from 1,000 through 1,000,000 digits, with
  median samples and full output equality checks.

### Changed

- Replaced the test-only floating-point finalization path with guarded GMP
  integer square root, division, rounding, and decimal formatting in production
  calculation code.
- Added 1,000-digit equality across 16, 32, and 64 guard digits to validate
  precision-policy stability independently of the fixed 100-digit reference.

## [0.10.0] - PR-0019

### Added

- Added a scheduler-aware Binary Splitting API with explicit sequential-cutoff
  and tasks-per-worker policy values.
- Added read-only detection of whether the current thread belongs to a given
  scheduler for deadlock-safe nested-call fallback.
- Added stopped, zero-worker, queue-rejection, worker-context, cutoff, exact
  result equality, and observable multi-worker execution coverage.

### Changed

- Changed parallel-compatible Binary Splitting from a sequential placeholder
  to a staged DAG of balanced leaf blocks and adjacent parallel merge levels.
- Kept task result slots internal to Binary Splitting instead of expanding the
  scheduler completion handle into a generic future.
- Execute final merge levels inline when fewer than two merge pairs are
  available, avoiding scheduler overhead without available parallelism.

## [0.9.0] - PR-0018

### Added

- Added in-place arbitrary-precision sign negation to `GMPInteger` for
  Chudnovsky leaf terms, including positive, negative, and zero coverage.
- Added consistent half-open Binary Splitting range validation to all public
  split entry points, with empty, reversed, and minimum valid range coverage.
- Added GMP-backed Chudnovsky leaf computation for P/Q/T terms, including the
  zero-term special case, alternating T sign, and known k=0, k=1, and k=2
  values.
- Added exact Chudnovsky P/Q/T regression values for `[0, 2)`, `[1, 3)`, and
  `[0, 3)`, including range metadata and public split-entry-point equality.
- Added a test-only 256-bit GMP integration check that converts the `[0, 3)`
  P/Q/T result into a Chudnovsky approximation and verifies 20 decimal places
  of pi without introducing production precision or output APIs.
- Added Binary Splitting boundary regression coverage for maximum `size_t`
  ranges and multi-level `[0, 16)` equality across all split entry points.

### Changed

- Changed sequential Binary Splitting from zero-valued placeholder leaves to
  mathematically correct Chudnovsky P/Q/T nodes.
- Documented the half-open range, leaf arithmetic, merge, and deferred
  parallel-execution contracts for Binary Splitting.

## [0.8.0] - PR-0017

### Added

- Added observable `Stopped`, `Running`, and `Stopping` scheduler lifecycle
  states.
- Added lifecycle tests for idempotent start and stop, restart, concurrent
  start and stop, and submission rejection while stopping.
- Added accepted outstanding-task accounting for deterministic drain shutdown.
- Added worker execution context tracking for global and local queue routing.
- Added tests for immediate drain, failed-task drain, bounded global queue
  rejection, and worker-local nested submission.
- Added cross-pool worker submission coverage to verify that only workers owned
  by the target pool may use its local queues.
- Added direct LockFreeQueue tests for capacity validation, bounded FIFO order,
  repeated slot reuse, concurrent producers, and exactly-once MPMC execution.
- Added a dedicated scheduler concurrency test target for deterministic
  submit/stop acceptance-boundary validation.
- Added direct coverage proving that concurrent producers' accepted tasks run
  exactly once while submissions made after `Stopping` are never executed.
- Added repeated multi-caller `start()` and `stop()` coverage across reusable
  ThreadPool lifecycle cycles.
- Added a dedicated observable work-stealing behavior test separate from the
  existing high-volume scheduler completion test.
- Added a root-only global test queue proving worker-created child tasks execute
  from a worker-local queue without global fallback.
- Added coverage that blocks the root worker and observes child execution on at
  least two distinct thief threads, with exactly-once execution and completed
  handles.

### Changed

- Serialized `ThreadPool` lifecycle transitions without holding the lifecycle
  lock while joining workers.
- Changed submissions to be accepted only while the pool is `Running`.
- Made worker startup publish `running` only after thread creation succeeds.
- Changed external submissions to use the bounded global queue and worker
  submissions to use the current worker's local queue.
- Changed shutdown to request every worker to stop before joining any worker.
- Changed workers to continue local, global, and steal execution until every
  accepted task reaches a terminal state.
- Changed LockFreeQueue sequence handling to retry stale producer and consumer
  positions while preserving acquire/release publication of task payloads.
- Changed `empty()` to inspect the next consumer slot instead of comparing
  reservation counters.

### Fixed

- Fixed races between concurrent scheduler start, stop, and submit operations.
- Fixed partial worker startup state when thread creation fails.
- Fixed shutdown leaving accepted tasks pending in worker or global queues.
- Fixed global queue capacity being bypassed by external submissions.
- Fixed producer and consumer contention being misreported as full or empty.
- Fixed invalid zero- and one-slot LockFreeQueue capacities being accepted.
- Removed unrelated trailing text from the LockFreeQueue implementation file.

## [0.7.0] - PR-0016

### Added

- Added a shared task completion state for scheduler task lifecycle tracking.
- Added move-only `TaskHandle` support for validity, waiting, completion, and
  failure observation.
- Added captured exception propagation through `TaskHandle` without allowing
  task exceptions to escape worker threads.
- Added direct inspection of a captured `exception_ptr` through `TaskHandle`.
- Added synchronization, rejection, failure, and multi-handle join tests.
- Added direct task lifecycle tests covering every state transition, waiter
  notification, exception isolation, and duplicate execution prevention.

### Changed

- Changed `ThreadPool::submit()` and `Scheduler::submit()` to return
  `TaskHandle`.
- Replaced scheduler test polling loops with explicit handle waits.
- Clarified `TaskHandle` shared-state ownership and added compile-time checks
  for its move-only contract.
- Removed the unimplemented `ThreadPool::submitLocal()` declaration so the
  public submission surface consistently uses `TaskHandle submit(Task)`.
- Reorganized scheduler synchronization tests into focused lifecycle, join,
  failure, rejection, custom-queue, and load scenarios.

### Fixed

- Fixed the incomplete TaskHandle integration that prevented scheduler targets
  from building.
- Fixed task failure handling so one throwing task does not terminate its worker
  thread.

## [0.6.1] - PR-0015

### Added

- Added `BinarySplitter::splitSequential()` API.
- Added `BinarySplitter::splitParallel()` API as the future scheduler integration entry point.
- Added validation tests to verify sequential and parallel split interfaces produce identical results.

### Changed

- Refactored recursive binary splitting into a dedicated sequential implementation.
- Updated `BinarySplitter::split()` to delegate to the sequential implementation.
- Prepared BinarySplitter architecture for future scheduler-based parallel execution.

### Fixed

- N/A

## [0.6.0] - PR-0014

### Added

* Added GitHub Actions CI pipeline.
* Added automated build and test verification on pull requests and main branch updates.
* Added GCC and Clang build matrix testing.
* Added clang-format validation.
* Added clang-tidy static analysis.
* Added AddressSanitizer and UndefinedBehaviorSanitizer checks.
* Added CI artifact upload for build output and test results.

### Changed

* Improved project build verification workflow.
* Updated CMake integration for automated testing support.
* Added CI-based validation path for contributors.

### Notes

* New contributors can now verify builds and tests automatically through GitHub Actions.
* Local build and test workflow is mirrored in CI environment.

### Fixed

* N/A

## [0.5.1] - PR-0013

### Added

- Added GitHub Actions CI workflow.
- Added automated build and test verification.

### Changed

- Integrated CTest workflow into CMake test system.
- Improved project validation process.

### Fixed

- N/A

## [0.5.0] - PR-0011

### Added

- Added BinaryNode data structure.
- Added BinarySplitting merge operation.
- Added BinarySplitting test coverage.

### Changed

- Extended CMake build system with binary test target.
- Added binary splitting foundation layer.

### Fixed

- N/A

## [0.5.0] - PR-0010

### Added

- Added GMPInteger wrapper.
- Added GMP based big integer abstraction.
- Added GMPInteger copy and move support.
- Added GMPInteger arithmetic operations.
- Added GMPInteger tests.

### Changed

- Extended bigint module foundation.
- Added GMP integration to build system.

### Fixed

- N/A

## [0.5.0] - PR-0009

### Added

- Added GMPInteger wrapper.
- Added GMP based Big Integer foundation.
- Added GMPInteger tests.

### Changed

- Added GMP build integration.
- Extended build system with bigint sources.

### Fixed

- N/A

## [0.4.0] - PR-0008

### Added

- Added WorkStealingQueue scheduler component.
- Added worker local queue execution path.
- Added worker stealing support.
- Added WorkStealingTest.

### Changed

- Extended Worker execution model with local queue processing.
- Updated ThreadPool worker coordination for work stealing.
- Extended scheduler execution flow for distributed task processing.

### Fixed

- Fixed missing work stealing scheduler validation coverage.

## [0.4.0] - PR-0007

### Added

- Added ThreadPool implementation.
- Added ThreadPool worker lifecycle management.
- Added ThreadPool tests.

### Changed

- Scheduler now delegates worker management to ThreadPool.
- Scheduler no longer manages Worker instances directly.
- Scheduler execution flow now uses ThreadPool abstraction.

### Fixed

- Fixed scheduler ownership separation between Scheduler and Worker layers.

## [0.4.0] - PR-0006

### Added

- Added scheduler queue abstraction.
- Added queue interface layer.
- Added ReferenceQueue implementation.
- Added LockFreeQueue implementation.

### Changed

- Updated Scheduler to support interchangeable queue backends.
- Updated Worker to operate with scheduler queue implementations.
- Separated scheduler architecture validation from queue optimization.

### Fixed

- Fixed LockFreeQueue enqueue position handling.
- Fixed LockFreeQueue dequeue position handling.
- Fixed scheduler queue replacement test coverage.

## [0.4.0] - PR-0005

### Added

- Added Scheduler foundation.
- Added Task abstraction.
- Added Worker implementation.
- Added Scheduler implementation.
- Added Reference Queue implementation.
- Added scheduler subsystem tests.

### Changed

- Extended CMake build system with scheduler sources.
- Added scheduler-test target.
- Split Scheduler development into Reference Queue and Lock-Free Queue phases.

### Fixed

- Fixed scheduler build integration.
- Fixed queue implementation architecture for staged development.

## [0.3.0] - PR-0004

### Added

- Added Memory Layer foundation.
- Added Arena allocator.
- Added Memory Pool allocator.
- Added Alignment utility.
- Added Scratch Buffer.
- Added memory subsystem tests.

### Changed

- Extended CMake build system with memory sources.
- Added memory-test target.

### Fixed

- N/A

## [0.2.0] - PR-0003

### Added

- Added runtime CPU feature detection.
- Added CPUID detection.
- Added AVX, AVX2, AVX512 detection.
- Added CPU information reporting.
- Added platform detection tests.

### Changed

- Extended CMake build system with platform test target.

## [0.1.0] - PR-0002-1

### Added

- Reorganized project documentation under `docs/`.
- Created initial scheduler module.
- Added `include/scheduler/ThreadPool.hpp`.
- Added `src/scheduler/ThreadPool.cpp`.
- Established namespace `pi::scheduler`.
- Added initial non-copyable/non-movable `ThreadPool` class skeleton for future implementation.

- Added configuration system foundation.
- Added `include/config/Defaults.hpp`.
- Added `include/config/Config.hpp`.
- Added `src/config/Config.cpp`.
- Added `include/config/ConfigLoader.hpp`.
- Added `src/config/ConfigLoader.cpp`.
- Added `include/config/CommandLine.hpp`.
- Added `src/config/CommandLine.cpp`.
- Added default configuration values.
- Added command-line configuration override support.
- Added `config.toml.example`.

- Added toml++ integration.
- Added `scripts/install-tomlpp.sh`.
- Added third-party dependency setup workflow for toml++.
- Added TOML configuration parsing support.
- Added configuration validation.


### Changed

- Updated CMake build configuration.
- Added toml++ include path.
- Enabled header-only toml++ usage.
- Updated ConfigLoader from stub implementation to TOML-based implementation.


### Fixed

- Fixed ConfigLoader temporary runtime failure state.
- Fixed missing TOML include integration after dependency installation.
- Fixed configuration loading workflow.


---

## [Unreleased]

### Added

- Logger system.
- Platform detection.
- Memory subsystem.
- Lock-free scheduler.
- Work stealing scheduler.
- Big integer backend.
- Binary splitting engine.
- Chudnovsky calculation core.
### Added

- Wired storage directory, memory budget, target chunk size, compression, and
  concurrent I/O settings through TOML/CLI configuration into `StoragePolicy`.
- Extended progress snapshots and reporters with resident storage bytes,
  durable stored bytes, and indexed chunk count.
- Added a configurable in-memory versus forced out-of-core merge benchmark
  reporting elapsed time, peak RSS, spill/reload counts, and spilled bytes.
- Defined the PR-0026 stabilization boundary: synchronous spill/reload and
  deterministic memory behavior remain in this PR; asynchronous writing,
  merge wait/backpressure, concurrent I/O, compression optimization, and
  NUMA/Huge Pages work are explicitly deferred to PR-0027/PR-0028.
- Wired validated runtime storage configuration into the production calculator
  path, including StorageManager, StorageMergeCoordinator, and ProgressTracker
  injection while preserving the disabled out-of-core path.
- Added a bounded asynchronous chunk writer with worker lifecycle tracking,
  durable completion handles, queued cancellation, shutdown drain/cancel
  modes, and serialized StorageManager publication.
- Extended NodeLifecycle to consume asynchronous stored/failed/cancelled
  completions while preserving resident payload ownership until durability.
- Connected the merge coordinator to the bounded writer: spill submissions
  wait for queue capacity, payloads remain resident until all durable handles
  complete, and async failures restore the lifecycle to resident.
- Added a bounded asynchronous chunk reader with queued/loading/loaded/
  failed/cancelled handles and serialized StorageManager reload access.
- Connected asynchronous reload to merge preparation: stored nodes are queued,
  awaited, decoded through the canonical BinaryNode/Chunk adapter, and only
  then returned to the resident lifecycle.
- Extended progress snapshots and text/JSON output with asynchronous storage I/O
  queue, active, completed, and failed counters for writes and reads.
- Added failure and shutdown coverage for duplicate durable writes, missing
  reload identities, terminal handle states, and post-shutdown submission
  rejection.
- Added an async/sync out-of-core merge benchmark mode. On the current runner,
  1,000,000 digits under a forced 1 MiB budget completed in 0.512 s sync and
  0.544 s async, with 15 spills and 15 reloads in both modes. The async run
  used 27 MiB peak RSS versus 23 MiB sync; this is a baseline, not a target.
- Stabilized PR-0027 with a clean 64-test regression run and separated
  concurrent I/O, compression optimization, and NUMA/Huge Pages work into the
  PR-0028 follow-up boundary.
- Added a staged PR-0028 high-performance roadmap covering reproducible
  measurement, concurrent I/O, data movement/compression, and NUMA/Huge Pages
  acceptance gates.
- Enabled real concurrent async storage execution: distinct chunk file
  operations can overlap while StorageManager index publication remains
  synchronized. The benchmark now accepts worker and queue arguments and uses
  process-isolated temporary directories.
- Reduced async spill data movement by moving Chunk payload ownership into the
  writer queue instead of copying GMP P/Q/T values at submission.
- Added a bounded LZ4 compression backend with durable ChunkStore round-trip,
  metadata validation, CRC coverage, and CI dependency setup. Full none-vs-LZ4
  workload benchmarking remains a PR-0029 follow-up.
- Added compression selection to the storage throughput benchmark. A current
  100 MiB single-chunk run measured none at 99.66 MiB and 3.27 s total versus
  LZ4 at 70.05 MiB and 2.74 s total; the full workload matrix remains required.

### Verification

- Verified the PR-0028 storage pipeline with a clean Debug build and all 64
  CTest targets passing.
- Verified the async multi-chunk path at 1,000,000 digits with four workers
  and queue capacity four: 15 spills, 15 reloads, 27 MiB peak RSS, and a
  successful P-bit result.
- Verified 512 MiB none/LZ4 single-chunk round trips and recorded the current
  platform audit: one NUMA node, no explicit HugeTLB pages, and no default
  affinity or system-wide THP override.
- Separated PR-0028 functional implementation from PR-0029 repeatable
  workload measurement, bottleneck optimization, and large-data acceptance.
- Fixed the PR-0029 measurement contract: Release/compiler populations,
  CPU/filesystem metadata, cold/warm cache separation, five repetitions,
  p50/p95 summaries, raw samples, queue telemetry, integrity, and P/Q/T
  equality are required before accepting an optimization. `/tmp` tmpfs and
  ext4 results are explicitly kept as separate populations.
- Added PR-0029 stage telemetry for GMP encode/decode, compression,
  file read/write, file sync, rename, directory sync, CRC32C, index
  publication, async queue-capacity wait, and async worker active time.
- Executed the PR-0029 sync/async multi-chunk workload matrix on separate
  tmpfs and ext4 populations. All 34 combinations preserved spill/reload
  counts and the P-bit result; ext4 durability sync is the dominant observed
  bottleneck. Single-chunk repetition and 1 GiB acceptance remain incomplete,
  and one non-reproducible 512 MiB ext4/LZ4 mismatch is tracked for follow-up.
- Removed duplicate read/decode work from verified reload while preserving
  checksum, structural, identity, compression, and GMP validation. Added
  separate aggregate telemetry for index mutex wait and index commit time.
  The 1,000,000-digit async benchmark retained 15 spills/reloads and the same
  P-bit result; reload telemetry dropped from roughly 19/21/15 ms to
  9/10/6 ms for file-read/CRC/GMP-decode in the repeated sample population.
- Completed 1 GiB release round-trip checks for none/LZ4 on both tmpfs and
  ext4. Chunk identity, index integrity, and payload equality passed in all
  four cases. ASan/UBSan passed the 100 MiB storage case and storage-manager
  regression tests; larger sanitizer runs were environment-limited and are
  not marked as accepted.
- Audited platform applicability: the current host has one NUMA node, no
  explicit HugeTLB pool, THP already enabled, and powersave CPU governors.
  NUMA/HugeTLB/THP code changes are deferred; affinity is reserved for
  benchmark reproducibility, while ext4 durability and index commit remain
  the actionable optimization targets.
- Completed the single-chunk matrix at five repetitions per cell: 12 cells,
  60/60 successful round-trips, with min/p50/p95 summaries for 100 MiB,
  512 MiB, and 1 GiB none/LZ4 on tmpfs and ext4. The earlier 512 MiB
  ext4/LZ4 mismatch did not recur.
- Added explicit cold/warm reload measurement mode and completed 60 cold plus
  60 warm samples across the same 12 cells. Warmup payloads and timings are
  excluded from measured reload telemetry; all 120 executions preserved
  chunk integrity and payload equality. Cache warmup did not consistently
  reduce wall-clock reload time because CRC/GMP decode remains significant.
- Added `peak_rss_mib` to the single-chunk throughput benchmark using
  `getrusage(RUSAGE_SELF)`. A 12-cell 100/512/1024 MiB none/LZ4 tmpfs/ext4
  baseline completed 12/12 successfully, with observed peaks of 591~620,
  2962~3112, and 5523~5945 MiB respectively. Five-sample RSS distribution
  acceptance remains separate from the instrumentation baseline.
- Rechecked the earlier 512 MiB ext4/LZ4 mismatch with 20 additional
  independent-directory Release runs. All 20 round-trips passed with no
  checksum, identity, or payload mismatch; the anomaly is currently
  non-reproducible and remains documented as an environment observation.
- Completed 512 MiB and 1 GiB none/LZ4 large-data round-trips under combined
  ASan/UBSan with leak detection disabled due the runner's ptrace limitation.
  All four cases passed without sanitizer diagnostics; peak RSS ranged from
  3506 MiB to 6948 MiB.
- Reduced ChunkCodec store-path data movement by writing none/LZ4 payloads
  directly into a bounded final output buffer. Release smoke round-trips for
  100 MiB, 512 MiB, and 1 GiB none/LZ4 all passed with canonical bytes and
  CRC integrity preserved. Repeated before/after speedup acceptance remains
  separate from this correctness and allocation reduction change.
- Completed single-chunk peak RSS five-sample acceptance for the full 12-cell
  workload: 100/512/1024 MiB × tmpfs/ext4 × none/LZ4, 60/60 successful
  round-trips with no checksum, identity, or payload mismatch. Per-cell p95
  RSS was 491~5,152 MiB (100 MiB/LZ4 through 1 GiB/none), below the recorded
  PR-0028 release baseline ranges of 591~620, 2,962~3,112, and 5,523~5,945
  MiB. Raw samples remain in `/tmp/givemepi-pr0029-before-after-complete-after.tsv`.
- Completed the independent Reader/Writer workload matrix: workers 1/2/4/8 ×
  queue capacities 1/4/16/64 × tmpfs/ext4 × none/LZ4, five repetitions per
  cell (64 cells, 320 runs). Every run completed with 8 writer successes,
  8 reader successes, and 16 indexed chunks. Elapsed-time p50 across all
  samples was 0.013 s and p95 was 0.215 s; raw samples are preserved in
  `/tmp/givemepi-pr0029-reader-writer-matrix.tsv`. The benchmark currently
  does not report peak RSS, so this is execution/correctness evidence, not a
  separate memory acceptance.
- Completed multi-chunk concurrent I/O cold/warm acceptance using sync and
  async modes, workers 1/2/4/8, queues 1/4/16/64, tmpfs/ext4, none/LZ4, and
  five repetitions: 256 cells and 1,280/1,280 successful runs. Every sample
  preserved 15 spills, 15 reloads, and the expected P-bit result; aggregate
  elapsed p50/p95 was 0.368/2.124 seconds and peak RSS p95 was 27 MiB.
  Cold and warm populations showed no correctness regression. Raw samples are
  preserved at `/tmp/givemepi-pr0029-multichunk-concurrent-cold-warm-accepted.tsv`.
- Finalized PR-0029: Debug rebuilt with all 64 CTest tests passing, Release
  benchmark targets rebuilt, and ASan/UBSan out-of-core smoke passed for sync
  and async 1,000,000-digit workloads with leak detection disabled. NUMA,
  Huge Pages, and affinity remain deferred on this one-NUMA/no-HugeTLB host;
  no further PR-0029 scope is open.
