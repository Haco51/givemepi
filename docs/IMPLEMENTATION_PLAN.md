# IMPLEMENTATION PLAN

## Purpose

This document contains approved implementation plans and future PR boundaries.

It is maintained separately from `PR-progress/`, which is reserved for the
developer's own work records.

Agent-authored plans are added or changed here only after user approval.

## Approved Execution Order

| PR | Goal | Status |
| --- | --- | --- |
| PR-0016 | Task synchronization foundation | Complete |
| PR-0017 | Scheduler correctness hardening | Complete |
| PR-0018 | Chudnovsky leaf and range validation | Complete |
| PR-0019 | Cutoff-based parallel Binary Splitting | Complete |
| PR-0020 | End-to-end Chudnovsky calculation | Complete |
| PR-0021 | P/Q/T checkpoint block foundation | Complete |
| PR-0022 | Checkpoint integrity verification | Planned |
| PR-0023 | Progress snapshot and reporting | Planned |
| PR-0025 | Runtime storage foundation | Complete |
| PR-0026 | Synchronous out-of-core stabilization | Complete |
| PR-0027 | Asynchronous storage pipeline | Complete |
| PR-0028 | Concurrent storage pipeline implementation | Complete |
| PR-0029 | Measurement and bottleneck optimization | Complete |

---

## PR-0016: Task Synchronization Foundation

### Goal

Add a shared completion state between `Task` and `TaskHandle` so callers can
wait for submitted work, observe completion or failure, and safely join
scheduler tasks.

This PR establishes synchronization only. Scheduler queue routing, drain
shutdown semantics, and Binary Splitting parallel execution remain separate
follow-up work.

### Architecture Contract

`Task` and its returned `TaskHandle` share one `TaskSharedState` through
`std::shared_ptr`.

The shared state owns:

- `TaskState` (`Pending`, `Running`, `Completed`, `Failed`)
- `std::exception_ptr`
- completion mutex
- completion condition variable

`TaskHandle` does not own or reference the `Task` object itself. It remains a
move-only observation handle over the shared completion state.

Task exceptions are captured in the shared state and must not escape the
worker thread. Callers inspect or rethrow failures through `TaskHandle`.

### Work Sequence

1. Define the shared task state.
2. Update `TaskHandle` to observe the shared state.
3. Update `Task` to publish state changes and captured failures.
4. Align `ThreadPool::submit()` and `Scheduler::submit()` on `TaskHandle`.
5. Return invalid handles for rejected submissions.
6. Preserve the existing queue and worker topology.
7. Add wait, completion, failure, rejection, and multi-handle join tests.
8. Update CMake if an out-of-line `TaskHandle.cpp` is added.
9. Build all targets, run CTest, and run `git diff --check`.
10. Update completion documentation.

### Files In Scope

- `include/scheduler/Task.hpp`
- `include/scheduler/TaskHandle.hpp`
- `src/scheduler/Task.cpp`
- `src/scheduler/TaskHandle.cpp` if required
- `include/scheduler/ThreadPool.hpp`
- `src/scheduler/ThreadPool.cpp`
- `include/scheduler/Scheduler.hpp`
- `src/scheduler/Scheduler.cpp`
- `tests/scheduler/SchedulerTest.cpp`
- `tests/scheduler/ThreadPoolTest.cpp`
- `CMakeLists.txt`
- scheduler documentation

### Tests

- valid handle after accepted submission
- invalid handle after rejected submission
- `wait()` blocks until completion
- completed state is observable
- failed state and captured exception are observable
- multiple handles can be joined without polling loops
- submission before start and after stop is rejected
- zero-worker submission is rejected

### Out of Scope

- LockFreeQueue redesign
- global/local queue routing changes
- drain or cancellation shutdown modes
- worker idle-wait optimization
- Binary Splitting parallel execution
- Chudnovsky leaf implementation
- checkpoint implementation
- custom FFT or NTT multiplication
- NUMA and Huge Page integration
- repository-wide formatting changes

### Definition of Done

- Project builds successfully.
- All existing and new synchronization tests pass.
- No task exception escapes a worker thread.
- Accepted tasks return valid handles.
- Rejected tasks return invalid handles.
- Callers can wait for multiple tasks without polling.
- Documentation matches the implemented behavior.

---

## PR-0017: Scheduler Correctness Hardening

### Goal

Stabilize scheduler lifecycle and queue semantics before parallel Binary
Splitting depends on them.

### Scope

- define drain shutdown semantics for accepted work
- route external submissions through the global queue
- route worker-created tasks through local queues
- make queue capacity and submission rejection observable
- add direct concurrent LockFreeQueue tests
- add observable work-stealing behavior tests
- preserve `ThreadPool` ownership of workers and the `IQueue` abstraction

### Lifecycle Contract

The scheduler exposes `Stopped`, `Running`, and `Stopping` states.

- A newly constructed scheduler is `Stopped`.
- `start()` transitions `Stopped` to `Running`.
- Calling `start()` while already `Running` is a no-op.
- `stop()` transitions `Running` through `Stopping` to `Stopped`.
- Calling `stop()` while already `Stopped` is a no-op.
- A concurrent `stop()` waits for the active stop generation to finish.
- A concurrent `start()` waits for `Stopping` to finish and then restarts.
- Restart after a completed stop is supported.
- Submission is accepted only in `Running`; `Stopped` and `Stopping` reject it.
- `running()` is true only in `Running`.

The lifecycle mutex is not held while worker threads are joined, preventing a
worker task that attempts submission during shutdown from deadlocking the stop
path. Work accepted before `Stopping` follows the drain contract below.

### Drain Shutdown Contract

`ThreadPool` counts every accepted task until a worker finishes executing it.
The count includes queued and currently executing work in both global and local
queues.

- The transition to `Stopping` rejects every new submission.
- Stop requests are published to every worker before any worker is joined.
- Workers continue local pop, global pop, and steal attempts while accepted
  work remains.
- A worker exits only after stop was requested and the accepted outstanding
  task count reaches zero.
- Failed tasks count as finished after their failure state is published.
- Queue insertion rejection rolls back its outstanding-task reservation.
- `stop()` returns only after all accepted handles are terminal and all workers
  are joined.
- A restarted pool begins with no outstanding work from the previous run.

The outstanding count is the authoritative drain condition. A transient
queue-empty observation is not sufficient because another worker may still be
executing an accepted task.

### Global and Local Queue Contract

- Calls from threads outside this pool publish to the bounded global `IQueue`.
- Calls from a worker owned by this pool publish to that worker's local queue.
- Calls from a worker owned by another pool are external submissions and use
  this pool's global queue.
- A full global queue returns an invalid `TaskHandle`.
- Local worker submission retains the same public `submit(Task)` API.
- Worker context is active only while the worker execution loop is running.

### Lock-Free Queue Correctness Contract

`LockFreeQueue` is a bounded multi-producer, multi-consumer queue. Its capacity
must be between two and `PTRDIFF_MAX`, inclusive. Capacity zero cannot index a
slot, and capacity one cannot distinguish the occupied and reusable sequence
states required by this queue protocol.

Each slot sequence is compared with the producer or consumer's expected
sequence:

- an equal sequence allows that operation to claim the position
- a sequence behind the expectation means full for a producer or empty for a
  consumer
- a sequence ahead of the expectation means the observed position is stale,
  so the operation reloads its shared position and retries

Queue positions are coordination counters and use relaxed atomic operations.
The per-slot sequence uses release when publishing a task or releasing a
consumed slot and acquire before reading that slot. This sequence edge protects
the non-atomic `Task` payload.

`empty()` examines the next consumer slot rather than comparing reservation
counters. It is a concurrent snapshot only and is never used as the drain
shutdown invariant; accepted outstanding-task accounting remains authoritative
for shutdown.

Direct tests cover invalid capacity, bounded FIFO behavior, repeated slot
reuse, concurrent producers with unused capacity, and concurrent producers and
consumers with exactly-once task execution.

### Direct Concurrency Test Contract

Scheduler concurrency tests use explicit gates and observable lifecycle states
instead of timing sleeps.

- All workers are first held inside accepted blocker tasks so the global queue
  and the `Stopping` state remain stable while producers race.
- Multiple external producers fill the queue while `Running`; every returned
  valid handle must become terminal and its task must execute exactly once.
- After `Stopping` is observed, the same producers submit another batch; every
  submission must return an invalid handle and none of those callables may run.
- Releasing the blockers allows shutdown to drain the accepted batch before
  returning.
- Multiple callers invoke `start()` and `stop()` together over repeated cycles;
  every cycle must expose one stable lifecycle state and complete all accepted
  handles before the stop callers return.

LockFreeQueue MPMC tests and scheduler lifecycle/drain tests remain separate
CTest targets so failures identify whether the queue algorithm or scheduler
integration violated its contract.

### Observable Work-Stealing Test Contract

Load completion and actual steal behavior use separate CTest targets.

- `WorkStealingTest` remains the high-volume scheduler completion regression.
- `WorkStealingBehaviorTest` supplies a test queue that accepts exactly one
  global root task and records every global push attempt.
- The root submits child tasks through the public scheduler API, placing them
  in its worker-local queue, and then blocks before it can pop that queue.
- Child tasks block when entered so at least two other worker threads must steal
  from the root's local queue before the test releases them.
- No child execution thread may equal the blocked root thread.
- The global queue must observe only the root push, proving child execution did
  not use a global-queue fallback.
- Every child handle must complete and every child callable must execute exactly
  once.

The test uses condition variables and thread identity observations rather than
sleep-based timing assumptions.

### Deferred Optimization

Worker idle-wait and wake-up optimization remains benchmark-driven follow-up
work unless required for scheduler correctness.

### Completion

PR-0017 was completed on 2026-07-20.

- Scheduler lifecycle, drain shutdown, and queue-routing contracts are
  implemented and documented.
- LockFreeQueue boundary and contention correctness have direct MPMC coverage.
- Scheduler concurrency and observable work-stealing behavior have dedicated
  CTest targets.
- GCC ASan/UBSan and Clang builds pass the full test suite.
- Concurrency and work-stealing tests pass more than 100 repeated runs.

### Next Contributor TODO

Begin PR-0018 with Chudnovsky leaf and `[start, end)` range correctness only.
Preserve the scheduler contracts established here, keep Binary Splitting
parallel execution deferred to PR-0019, and add known leaf and small-range
P/Q/T expectations before changing the parallel path.

---

## PR-0018: Chudnovsky Leaf and Range Validation

### Goal

Turn sequential Binary Splitting from a structural placeholder into a correct
Chudnovsky P/Q/T computation.

### Binary Splitting Contract

Every Binary Splitting range uses the half-open interval `[start, end)`.
Public split entry points reject empty or reversed ranges where `start >= end`
before performing unsigned range arithmetic or entering recursion.

For the single-term range `[k, k + 1)`, the leaf values are:

- when `k == 0`, `P = 1`, `Q = 1`, and `T = 13591409`
- when `k > 0`, `P = (6k - 5)(2k - 1)(6k - 1)`
- when `k > 0`, `Q = k^3 * 10939058860032000`
- when `k > 0`, `T = P * (13591409 + 545140134k)`
- `T` is negated when `k` is odd

Leaf coefficient arithmetic is performed with `GMPInteger` after converting
the term index. Intermediate coefficient products must not be evaluated in a
fixed-width integer type where they can overflow before reaching GMP.

Adjacent nodes `[a, m)` and `[m, b)` merge into `[a, b)` using:

- `P = P_left * P_right`
- `Q = Q_left * Q_right`
- `T = T_left * Q_right + P_left * T_right`

`split()` remains the default sequential entry point and delegates to
`splitSequential()`. `splitParallel()` must return the same mathematical result
and enforce the same range contract, but scheduler-backed execution, cutoff
selection, and parallel merge remain deferred to PR-0019.

### Scope

- validate `[start, end)` input ranges
- implement Chudnovsky leaf P/Q/T values
- test known leaf and small-range P/Q/T results
- add a small known-digits integration test
- preserve the existing merge equation and sequential API

### Completion

PR-0018 was completed on 2026-07-20.

- Sequential Binary Splitting now produces Chudnovsky P/Q/T leaf values.
- Every public split entry point rejects empty and reversed ranges.
- Leaf, small-range, upper-boundary, and multi-level recursive results have
  direct regression coverage.
- A test-only GMP calculation verifies 20 decimal places of pi from `[0, 3)`.
- GCC, Clang, ASan, and UBSan pass the full test suite; LeakSanitizer is not
  runnable under the workspace ptrace restrictions.

### Next Contributor TODO

Begin PR-0019 by defining and measuring a sequential cutoff before introducing
scheduler submissions. Preserve exact equality with `splitSequential()`, join
every submitted child through `TaskHandle`, and retain a sequential fallback
for small ranges and rejected submissions.

---

## PR-0019: Cutoff-Based Parallel Binary Splitting

### Goal

Use the stabilized scheduler to execute large Binary Splitting ranges in
parallel while keeping small ranges sequential.

### Scope

- define a configurable or measured sequential cutoff
- submit only sufficiently large child ranges
- join child `TaskHandle` values before merge
- preserve sequential fallback
- test sequential and parallel result equality with real Chudnovsky values
- measure scheduling overhead before tuning the cutoff

### Parallel Execution Contract

PR-0019 uses a staged Binary Splitting DAG instead of recursive worker tasks
that submit children and block. The current scheduler does not execute queued
work while a worker waits on a `TaskHandle`; recursive fork/join could therefore
leave every worker waiting while child tasks remain queued.

The caller partitions `[start, end)` into balanced sequential leaf blocks. The
number of blocks is bounded by the worker count so task metadata and scheduler
overhead do not grow linearly with a very large term count. Each accepted task
writes one exclusively owned result slot and never waits for another task.

After all leaf blocks are complete, adjacent results are merged in parallel
levels until one root remains. The caller joins each complete level before
starting the next, making dependencies explicit and providing natural future
boundaries for checkpointing, progress reporting, NUMA placement, and memory
reclamation.

The public parallel API receives a `Scheduler&` and an explicit execution
policy containing the sequential cutoff and target tasks per worker. A
cutoff-only convenience overload retains a measured default task multiplier.
It does not use a global scheduler or read application configuration.
If the range is below the cutoff, the scheduler is stopped or has no workers,
the call originates from one of the scheduler's own workers, or an individual
submission is rejected, affected work executes synchronously. Every path must
produce exactly the same P/Q/T result as `splitSequential()`.

Task failures are rethrown by the caller after joining their handles. Result
storage remains internal to Binary Splitting; PR-0019 does not turn the
scheduler completion-only `TaskHandle` into a generic future.

### Work Sequence

1. Expose read-only detection of the scheduler's current worker context.
2. Add the scheduler-aware `splitParallel()` overload and validate cutoff.
3. Partition large ranges into a bounded set of balanced sequential blocks.
4. Execute leaf blocks through accepted tasks or synchronous fallback.
5. Join and rethrow task failures before consuming result slots.
6. Execute adjacent merge pairs as staged parallel levels.
7. Test exact equality, cutoff behavior, stopped/zero-worker fallback,
   submission rejection, worker-call fallback, and observable concurrency.
8. Measure representative task sizes before selecting a production default.

### Completion

PR-0019 was completed on 2026-07-20.

- Large ranges execute as bounded sequential leaf blocks followed by staged
  parallel merge levels.
- Tasks never wait for child tasks, avoiding worker-starvation deadlocks.
- Stopped, zero-worker, owned-worker, and rejected-submission paths fall back
  synchronously without changing P/Q/T results.
- Tests observe task acquisition by multiple worker threads and verify exact
  sequential equality across cutoff and uneven-range cases.
- The task count is bounded by the configurable tasks-per-worker policy and the
  final merge levels execute inline when no parallel pair work remains.
- A diagnostic four-worker `-O2` measurement showed scheduler overhead at 64
  terms and useful speedup by 256 terms; the cutoff remains explicit until the
  production workload and target CPUs have a benchmark suite.

### Next Contributor TODO

Begin PR-0020 with the minimum end-to-end Chudnovsky calculation path. Convert
requested digits to a guarded term count, execute the existing Binary
Splitting engine, finalize pi with integer fixed-point arithmetic, and verify
known decimal output before defining checkpoint computation identity.

---

## PR-0020: End-to-End Chudnovsky Calculation

### Goal

Turn the verified P/Q/T engine into a minimal production calculation path from
requested decimal digits to a normalized pi string.

### Arithmetic Contract

The finalization path remains integer based. For requested digits `d`, compute
with explicit guard digits, derive the required Chudnovsky term count, and use
the existing sequential or staged-parallel Binary Splitting engine.

The square-root factor is produced with GMP integer square root at the guarded
decimal scale. Final division, guard removal, and decimal formatting operate on
integers so correctness does not depend on host floating-point behavior or an
implicit global GMP floating-point precision.

### Scope

- define requested-digit, guard-digit, and term-count policy
- add required GMPInteger power-of-ten, square-root, and division operations
- implement `426880 * sqrt(10005) * Q / T` at guarded fixed-point precision
- normalize output as `3.<requested digits>`
- expose a computation API independent from CLI, checkpoint, and reporting
- support sequential and scheduler-backed Binary Splitting execution
- test known digits across multiple requested precisions
- add an end-to-end benchmark separating split, finalize, and format time

### Out of Scope

- checkpoint files and resume
- progress reporters
- BBP verification and final output hashing
- automatic CPU topology and cutoff tuning
- out-of-core decimal conversion

### Definition of Done

- requested digits deterministically map to sufficient guarded terms
- sequential and parallel calculation produce identical normalized output
- known pi prefixes pass at multiple precision boundaries
- invalid digit, guard, and scheduler policy inputs are rejected
- end-to-end benchmark results are recorded before further optimization
- the resulting computation identity fields are documented for PR-0021

### Completion

PR-0020 was completed on 2026-07-20.

- Requested digits map deterministically to guarded working digits, a
  conservative term count, and estimated binary precision without
  floating-point policy arithmetic.
- Sequential and staged-parallel calculation return identical normalized,
  half-up-rounded decimal strings with their immutable precision identity.
- Exact known rounded values pass from 1 through 100 decimal places, and 1,000
  digits remain identical across 16, 32, and 64 guard digits.
- GCC, Clang, ASan, UBSan, and analyzer checks pass the full suite.
- An opt-in Release benchmark records split, integer finalization, formatting,
  and total time through one million digits while validating full output.

### Computation Identity for PR-0021

Checkpoint identity must include requested digits, guard digits, working
digits, term count, arithmetic-format version, Binary Splitting range, and
parallel policy where it affects block boundaries. Runtime timing values are
observations and are not part of computation identity.

### Next Contributor TODO

Begin PR-0021 with a versioned P/Q/T checkpoint block format derived from this
computation identity. Keep worker threads free of disk I/O and preserve the
staged leaf-block boundary as the initial checkpoint granularity.

---

## PR-0021: P/Q/T Checkpoint Block Foundation

### Goal

Define a durable, versioned checkpoint block for one completed Binary Splitting
range without implementing resume policy yet.

### Scope

- versioned block header with file magic
- target computation identity
- range `[a, b)` and tree level
- P/Q/T payload lengths
- checksum type and checksum value fields
- GMP integer serialization and deserialization
- dedicated checkpoint writer path
- temporary-file write, flush, synchronization, and atomic rename
- versioned manifest format
- round-trip and interrupted-write tests

### Computation Identity Contract

Checkpoint compatibility is based on mathematical identity, not execution
policy. The identity contains:

- algorithm identifier and arithmetic-format version
- requested digits and guard digits
- working digits and Chudnovsky term count
- Binary Splitting formula/version identifier

Worker count, cutoff, tasks per worker, timing values, and queue capacity do not
change P/Q/T and therefore are not compatibility fields. They may be recorded
as optional provenance. Each block's `[start, end)` and tree level define its
position independently of the parallel partition policy that produced it.

### Canonical Block Format

The codec writes fields explicitly and never dumps a native C++ struct. All
fixed-width integers use big-endian byte order. Version 1 contains:

1. fixed magic and format version
2. encoded header size and flags
3. mathematical computation identity
4. `[start, end)` and tree level
5. canonical P/Q/T sign and magnitude lengths
6. checksum algorithm, checksum length, and checksum value
7. canonical P/Q/T payloads

GMP values use signed magnitude encoding. Zero has sign `zero` and an empty
magnitude; positive and negative values use a minimal unsigned big-endian
magnitude with no leading zero bytes. The decoder rejects non-canonical values
instead of silently normalizing them.

The initial checksum algorithm is CRC32C. It covers the canonical header with
the checksum bytes zeroed plus the complete P/Q/T payload. CRC32C protects
against accidental storage corruption; it does not replace PR-0022 independent
modular P/Q/T verification.

### Atomic Commit Contract

Block and manifest updates use a temporary file in the destination directory:

1. encode the complete content and checksum
2. create a unique temporary file without replacing another writer's temp file
3. write every byte and handle short writes
4. flush and synchronize the file
5. close the file successfully
6. atomically rename it to the deterministic final path
7. synchronize the parent directory

No final filename is visible before the full file is durable. A failed write,
sync, close, or rename leaves the previous final file untouched and removes or
leaves only an ignorable temporary file. Tests use injected commit fault points
rather than process timing races.

### Manifest Contract

The versioned manifest is an index, not proof that a block is reusable. It
contains the computation identity and deterministic entries with range, level,
filename, payload size, checksum metadata, and completion state. Entries are
sorted by range and level before encoding so identical state produces identical
bytes. Manifest replacement uses the same atomic commit protocol as blocks.

PR-0022 remains responsible for validating block structure, checksums,
manifest gaps/overlaps, and modular P/Q/T residues before resume trusts an
entry.

### Work Sequence

1. Define version constants, limits, computation identity, block metadata, and
   manifest entry value types without filesystem behavior.
2. Add canonical GMP signed-magnitude export/import and boundary tests for
   zero, positive, negative, and very large values.
3. Implement endian-safe primitive encoding/decoding with checked size
   arithmetic and no native struct serialization.
4. Implement CRC32C and fixed checksum-coverage fixtures.
5. Encode/decode an in-memory version-1 P/Q/T block and verify byte-stable
   round trips.
6. Implement same-directory temporary write, file sync, atomic rename, and
   parent-directory sync behind one storage component.
7. Add deterministic block naming and a synchronous `CheckpointStore` called
   only outside scheduler worker tasks.
8. Implement deterministic manifest encoding and atomic replacement.
9. Add fault-injected interrupted-write tests proving the old final file is
   preserved and partial files are never accepted as final.
10. Run GCC, Clang, ASan, UBSan, analyzer, filesystem round-trip, and repeated
    atomic-commit tests before documenting completion.

### Boundary

Worker threads never perform checkpoint disk I/O. PR-0021 provides a
synchronous checkpoint storage path that the stage coordinator may call only
after completed nodes have been joined. A dedicated asynchronous writer thread
may wrap this path later without changing the block codec or commit protocol.

PR-0021 writes and round-trips checkpoint data but does not implement resume,
trust decisions, quarantine, corruption recovery, compression, or worker-stage
integration.

The presence of a block file does not make it reusable. PR-0022 performs all
validation before resume accepts it.

### Definition of Done

- A completed P/Q/T range round-trips without value loss.
- Partial files never receive a final checkpoint name.
- The header can be extended through format versioning.
- Checksum metadata is part of the initial format.
- Existing computation paths remain independent from storage details.
- Canonical bytes are independent of host endianness and struct layout.
- Failed atomic commits cannot replace an existing valid final file.
- Equivalent manifests encode to identical bytes regardless of insertion order.

### Completion

PR-0021 was completed on 2026-07-20.

- Added portable computation identity, block location, and execution provenance
  value types with mathematical compatibility validation.
- Added canonical signed-magnitude GMP serialization and a versioned,
  endian-independent P/Q/T block codec protected by CRC32C.
- Added synchronous deterministic checkpoint storage with same-directory temp
  files, file and directory synchronization, and atomic replacement.
- Added a canonical versioned manifest with deterministic ordering, CRC32C,
  and atomic replacement.
- Verified byte-stable memory and filesystem round trips, corruption rejection,
  pre-rename fault preservation, GCC, Clang, ASan, UBSan, and static analysis.

---

## PR-0022: Checkpoint Integrity Verification

### Goal

Allow resume to trust only structurally valid, storage-valid, and
mathematically verified P/Q/T blocks.

### Validation Pipeline

1. Validate file magic and supported format version.
2. Validate computation identity, range, and tree level.
3. Validate P/Q/T payload boundaries and lengths.
4. Validate payload checksum.
5. Validate manifest membership, overlap, gaps, and completion state.
6. Independently recompute P/Q/T residues modulo selected verification primes.
7. Accept the block or quarantine it and reschedule its range.

### Resume Rules

- Ignore incomplete temporary files.
- Never merge an unvalidated block.
- Recalculate missing, corrupt, incompatible, or mathematically invalid ranges.
- Reconstruct progress from accepted blocks instead of a saved percentage.
- Preserve format-version compatibility rules in one validation component.

### Tests

- truncated header and payload
- unsupported version
- mismatched target identity
- invalid range or tree level
- checksum mismatch
- manifest overlap and gap
- valid checksum with invalid modular P/Q/T result
- interrupted temporary file
- mixed valid and invalid blocks during resume

### Definition of Done

- Resume schedules only ranges not covered by validated blocks.
- Invalid data cannot enter the merge tree.
- Every rejection has a diagnostic reason.
- Validation is independent from CLI presentation.

---

## PR-0023: Progress Snapshot and Reporting

### Goal

Expose reliable computation progress to the CLI while preserving an extension
boundary for JSON, status files, GUI, metrics, and remote monitoring.

### Architecture

```text
Computation / Scheduler / Checkpoint
                |
                v
        ProgressTracker
                |
                v
       ProgressSnapshot
                |
       +--------+--------+
       |                 |
       v                 v
CLI Text Reporter   CLI JSON Reporter
                          |
                          v
                 Future Reporter Types
```

### Snapshot Fields

- schema version
- computation phase
- target digits
- total and completed Chudnovsky terms
- total and completed checkpoint blocks
- current merge level
- active and queued task counts
- elapsed time
- throughput
- optional ETA
- memory use
- checkpoint bytes
- last validated checkpoint
- terminal completion or failure state

Raw counters are authoritative. Percentage, throughput, and ETA are derived
values and may be unavailable until enough samples exist.

### Configuration and CLI

- TOML: `progress`, `progress_interval`, `progress_format`
- CLI: `--progress`, `--no-progress`
- CLI: `--progress-interval <milliseconds>`
- CLI: `--progress-format text|json`
- run shared configuration validation after command-line overrides

### Reporting Rules

- computation workers never format or print progress
- reporting runs on a dedicated path
- TTY text may update one line
- non-TTY text emits complete timestamped records
- JSON output uses a versioned machine-readable schema
- slow reporters cannot block computation workers
- resume reconstructs counters from PR-0022 validated blocks

### Tests

- monotonic raw progress counters
- stable immutable snapshots under concurrent updates
- phase transitions
- unknown ETA before sufficient samples
- text and JSON reporter output
- TTY and non-TTY behavior
- progress disabled mode
- interval and format validation
- resume reconstruction from validated block metadata
- slow reporter isolation

### Definition of Done

- CLI users can continuously see phase and completed work.
- Automation can consume versioned JSON progress.
- No algorithm or scheduler component depends on CLI formatting.
- Future reporter types require no changes to computation code.

---

## PR-0024: Final Output Verification

### Goal

Publish a final decimal result only after independent structural, cryptographic,
and mathematical verification, while supporting bounded-memory stored-file
inspection and durable external verification evidence.

### Implemented Architecture

```text
canonical output inspection
        |
        +--> versioned known digits
        +--> streaming SHA-256
        +--> GMP decimal-to-hex extraction
                         |
                         +--> deterministic BBP samples
        |
        v
FinalVerificationReport
        |
        v
versioned CRC32C verification manifest
```

### Contracts

- memory output is exactly `3.` plus requested ASCII digits;
- stored output adds exactly one LF and no other trailing bytes;
- SHA-256 excludes the LF;
- known digits are rounded at the requested precision;
- BBP positions are zero-based hexadecimal fractional positions;
- decimal-to-hex uses exact GMP integer arithmetic and a half-ULP interval;
- unresolved numeric boundaries are inconclusive, never guessed;
- malformed structure stops mathematical verification;
- known-digit failure does not suppress diagnostic hashing or BBP checks;
- passed manifests use canonical version-1 bytes, CRC32C, and atomic storage;
- manifest hash revalidation detects arbitrary stored-output mutation;
- CLI progress flows through writing, verifying, then terminal completion.

### Validation

- normal 1, 10, 50, 100, and 1,000-digit calculations;
- malformed structure, known-prefix mutation, BBP mismatch and inconclusive;
- middle and final file-digit mutation against manifest SHA-256;
- SHA-256 and known hexadecimal standard references;
- canonical manifest round-trip, corruption, and atomic failure injection;
- text and JSON `verifying_output` reporting;
- progress-enabled and disabled output equality;
- 128 MiB bounded-memory streaming inspection and hashing;
- GCC ASan/UBSan 49-test suite, Clang cross-build, and scan-build;
- Release verification benchmark at 1,000, 10,000, and 100,000 digits.

### Definition of Done

- every CLI result is verified by default before terminal success;
- success reports canonical SHA-256 and verification manifest path;
- verification failures retain structured stage diagnostics;
- external file revalidation can compare against manifest SHA-256;
- verification does not depend on calculator internal state;
- large file structure and hashing remain bounded-memory.

### Next Contributor TODO

- Do not change the final-output or manifest format without a new version and
  compatibility decision.
- Select the next PR boundary with the user before implementation.
- If large-scale verification latency becomes a priority, benchmark parallel
  execution of independent BBP samples while keeping algorithms thread-free.

---

## PR-0028: Concurrent Storage Pipeline Implementation

### Goal

Implement concurrent storage execution without changing the PR-0027 durable
storage, lifecycle, progress, or synchronous fallback contracts.

### Approved Scope

- protect StorageManager index publication and in-flight identities while
  allowing file operations to overlap;
- remove unnecessary manager-wide serialization from async reader/writer
  workers;
- move encoded Chunk ownership into the async spill queue where safe;
- add a bounded LZ4 codec with durable round-trip and CRC validation;
- audit platform applicability without enabling unmeasured NUMA, HugeTLB, THP,
  or affinity policy by default.

### Verification

- full build and 64-test CTest suite;
- sync/async P/Q/T equality and lifecycle failure coverage;
- async multi-chunk spill/reload benchmark;
- none/LZ4 durable round-trip and metadata validation;
- `git diff --check` and documentation synchronization.

### Definition of Done

- Functional changes are separated from PR-0029 measurement-only changes.
- PR-0028 docs, checklist, changelog, and decisions match the implementation.
- The verified change set is committed and pushed before PR-0029 begins.

### Next Contributor TODO

Split out any PR-0029-only timing or benchmark changes, run the final PR-0028
verification command set, then commit and push PR-0028. Do not begin CRC,
buffer, queue, or platform tuning in this PR.

---

## PR-0029: Measurement and Bottleneck Optimization

### Goal

Build a reproducible performance baseline on top of the committed PR-0028
pipeline, identify stage-level bottlenecks, and apply only benchmark-proven
optimizations.

### Approved Sequence

1. Fix Release environment and workload/acceptance metadata.
2. Complete file, CRC32C, codec, GMP, index, and queue telemetry. **Complete**
3. Run 100 MiB/512 MiB/1 GiB single- and multi-chunk matrix with sync/async,
   worker 1/2/4/8, queue 1/4/16/64, cold/warm cache, five repetitions, and
   p50/p95 summaries.
4. Analyze CRC/read/GMP/buffer/queue/peak-RSS bottlenecks.
5. Implement bounded buffer reuse, streaming, CRC/read/GMP improvements, and
   queue adjustments one at a time.
6. Validate large-data Release, ASan/UBSan, restart/integrity, and sync/async
   equivalence.
7. Evaluate NUMA, Huge Pages, and affinity only on suitable hardware and only
   when measurements show a benefit.

Steps 1 and 2 are fixed in the PR-0029 measurement contract recorded in this
section and are implemented in the current working tree. Completion history,
architectural decisions, and task state remain in `docs/CHANGELOG.md`,
`docs/DECISIONS.md`, and `docs/CHECKLIST.md`. The bounded buffer and
CRC/read/GMP combined end-to-end before/after acceptance is complete from the
preserved 12-cell raw population. Baseline stage telemetry is unavailable, so
individual CRC/read/GMP attribution remains limited. Peak RSS, independent
Reader/Writer execution, and multi-chunk repeated cold/warm acceptance are
complete; only final stabilization and the final performance decision remain.

### Canonical PR-0029 Remaining Sequence

The remaining work is maintained in this file; separate agent-authored
`PR-0029-*` plan or report files are not required.

1. **Bounded buffer/data movement before/after — Complete**
   - Compare the PR-0028 baseline and current PR-0029 Release build on
     100/512/1024 MiB × none/LZ4 × tmpfs/ext4, five samples per cell.
   - Record store/load/total p50 and p95, encoded bytes, peak RSS,
     compression/decompression stage time, canonical bytes, and P/Q/T equality.
   - Accept only when correctness and memory are not worse and repeated p50/p95
     results support the claimed improvement. Otherwise classify it as a
     correctness/allocation optimization, not a latency speedup.

2. **CRC/read/GMP before/after — Complete as combined end-to-end evidence**
   - Reuse the same 12 single-chunk cells and compare file-read, CRC, GMP
     encode/decode, load, and total stage p50/p95 against baseline.
   - Keep SSE4.2 and portable fallback populations separate; retain CRC vectors,
     segmented chaining, and zero/signed GMP fixtures as correctness gates.
   - If stage latency does not improve repeatedly, record the change as
     correctness-only rather than claiming a speedup.

3. **Single-chunk peak RSS distribution — Complete**
   - Run the 12 single-chunk cells five times and calculate min/p50/p95/max for
     `peak_rss_mib`, with stored bytes and integrity status.
   - **Complete:** 60/60 samples succeeded. Per-cell p95 RSS was 491 MiB
     (100 MiB/LZ4) to 5,152 MiB (1 GiB/none), below the recorded PR-0028
     release baseline ranges of 591~620, 2,962~3,112, and 5,523~5,945 MiB.

4. **Independent Reader/Writer repeated matrix — Execution complete**
   - Run `reader-writer-concurrency-benchmark` for workers 1/2/4/8,
     queue 1/4/16/64, tmpfs/ext4, and none/LZ4, five repetitions per cell.
   - **Complete:** 64 cells and 320 runs completed. All runs reported
     `writer_success=8`, `reader_success=8`, and `indexed_chunks=16`; the
     raw population is preserved at
     `/tmp/givemepi-pr0029-reader-writer-matrix.tsv`. The current benchmark
     does not emit peak RSS, so memory acceptance remains separate from this
     execution result.
   - Record success counts, final index cardinality/identity, throughput,
     queue wait count/time, active time, failures, and peak RSS.
   - Do not select a worker/queue configuration from one fastest sample;
     require p50/p95 and bounded-memory evidence.

5. **Multi-chunk concurrent I/O and cold/warm acceptance — Complete**
   - Run sync and async out-of-core workloads with workers 1/2/4/8 and queues
     1/4/16/64, separated by tmpfs/ext4, none/LZ4, and cold/warm populations.
   - **Complete:** 256 cells × five repetitions, 1,280/1,280 successful.
     Every run preserved 15 spills, 15 reloads, and the expected P-bit result;
     aggregate elapsed p50/p95 was 0.368/2.124 seconds and RSS p95 was
     27 MiB. Raw samples are preserved at
     `/tmp/givemepi-pr0029-multichunk-concurrent-cold-warm-accepted.tsv`.
     Cold/warm populations showed no correctness regression.
   - Record elapsed time, spill/reload count, P-bit result, stage telemetry,
     queue telemetry, index commit, and RSS.
   - Preserve durability sync; classify ext4 sync dominance as a bottleneck,
     not as permission to weaken fsync or atomic publication.

6. **Final PR-0029 performance decision and stabilization — Complete**
   - Group raw results by compiler, CPU, filesystem, codec, and workload;
     summarize failed samples, outliers, p50/p95, RSS, integrity, and telemetry
     in the canonical changelog/checklist/decision records.
   - **Complete:** Debug build and all 64 CTest tests passed; Release
     benchmark targets rebuilt; ASan/UBSan out-of-core smoke passed for sync
     and async 1,000,000-digit runs with leak detection disabled. The final
     matrix summaries and raw populations are recorded above.
   - Marked changes as accepted, correctness-only, or deferred. NUMA/Huge
     Pages/affinity remains explicitly deferred because the current host has
     one NUMA node and no HugeTLB pool. PR-0029 is frozen pending commit.

#### Common acceptance gates

- Use a fixed PR-0028 baseline and current PR-0029 build with identical
  compiler, CPU, kernel, filesystem, governor, and affinity metadata.
- Keep tmpfs and ext4 populations separate and preserve raw samples with each
  summary.
- Require five samples per cell and p50/p95; a single fastest result is not
  evidence of optimization.
- Any checksum, identity, P/Q/T, restart/index, partial-file, lifecycle,
  memory-budget, or durability failure rejects the affected optimization.

### Next Contributor TODO

PR-0029 is complete and frozen pending commit. Preserve the raw benchmark
populations and do not add further optimization scope here; NUMA/Huge Pages/
affinity work belongs to a later PR if suitable hardware and measurements
justify it.
