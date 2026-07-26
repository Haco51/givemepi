# DECISIONS.md

This document records all architectural decisions.

Rules

- Never rewrite history.
- Never delete previous decisions.
- Decisions are append-only.
- If a decision changes, create a new record explaining why.

---

## ADR-0001

Date

2026-06-28

Status

Accepted

Title

Use Chudnovsky Formula

Decision

Use the Chudnovsky Formula as the primary π algorithm.

Reason

Currently one of the fastest known series for π.

Alternatives

- Gauss-Legendre
- Borwein
- Ramanujan

Consequence

Requires Binary Splitting.

---

## ADR-0002

Status

Accepted

Title

Use Binary Splitting

Decision

Use Binary Splitting instead of direct summation.

Reason

Significantly reduces complexity.

Consequence

Tree-based scheduler required.

---

## ADR-0003

Status

Accepted

Title

Use GMP

Decision

Delegate Big Integer multiplication to GMP.

Reason

GMP already implements

- Karatsuba
- Toom Cook
- FFT

Better than maintaining custom implementation.

---

## ADR-0004

Status

Accepted

Title

No OS Swap

Decision

Do not rely on operating system swap.

Reason

Swap is unpredictable.

Implement dedicated Out-of-Core Storage Layer.

Consequence

Need Storage Manager.

---

## ADR-0005

Status

Accepted

Title

Checkpoint System

Decision

Support resumable checkpoints.

Reason

Large computations may run for days.

Consequence

Checkpoint Manager required.

---

## ADR-0006

Status

Accepted

Title

SIMD Scope

Decision

Use SIMD only for memory operations.

Reason

GMP already provides optimized arithmetic.

Consequence

SIMD limited to

- memcpy

- limb operations

- prefetch

- compare

---

## ADR-0007

Status

Accepted

Title

Documentation First

Decision

Implementation is incomplete until documentation is updated.

Reason

Multiple AI agents participate.

Documentation synchronization prevents duplicated work.

Consequence

Every completed task updates

CHECKLIST.md

CHANGELOG.md

DECISIONS.md (if required)

ROADMAP.md (if required)

---

## ADR-0008

Date

2026-06-29

Status

Accepted

Title

Use toml++ for Configuration Parsing

Decision

Use toml++ as the TOML configuration parser.

Reason

The project requires a lightweight configuration system with:

- Human readable configuration files
- Type-safe value loading
- Header-only integration
- Simple dependency management

Alternatives

- Manual parser implementation
- Other TOML libraries

Consequence

ConfigLoader depends on toml++.

Third-party dependency is managed through:

scripts/install-tomlpp.sh

---

## ADR-0009

Date

2026-06-29

Status

Accepted

Title

Configuration Priority Order

Decision

Configuration values are applied in this order:

Default Values

↓

config.toml

↓

Command Line Arguments

Reason

Provides safe defaults while allowing persistent configuration and runtime overrides.

Consequence

ConfigLoader and CommandLine must preserve this priority order.

---

## ADR-0010

Date

2026-06-29

Status

Accepted

Title

PR Based Changelog Versioning

Decision

Every completed PR must create a version entry in CHANGELOG.md.

Format:

## [VERSION] - PR-ID

Reason

Multiple agents work on the project.

Versioned changelog entries improve:

- Change tracking
- Release management
- Collaboration

Consequence

PR completion requires documentation updates.

---

### ADR-0011

Runtime CPU Feature Detection

---

## ADR-0012

Date

2026-06-29

Status

Accepted

Title

Memory Layer Design

Decision

Use dedicated memory management components instead of relying only on general heap allocation.

Components:

- Arena Allocator
- Memory Pool
- Alignment Manager
- Scratch Buffer


Reason

Large π computations require predictable memory allocation patterns.

Frequent heap allocations can cause:

- fragmentation
- allocation overhead
- cache inefficiency


Consequence

Future computation modules should use the memory layer for temporary and high-frequency allocations.

Binary Splitting and Big Integer integration will use these components where appropriate.

---

## ADR-0013

Date

2026-06-29

Status

Accepted

Title

Template Implementation Separation

Decision

Template-based scheduler components keep declarations in header files and implementations in separate .inl files.

Reason

Scheduler components such as Lock-Free Queue require templates while maintaining readable interfaces.

Keeping all implementation inside headers increases file size and reduces maintainability.

Consequence

Template scheduler components will use:

include/scheduler/*.hpp
include/scheduler/*.inl

instead of placing implementation in .cpp files.

---

## ADR-0014

Date

2026-06-29

Status

Accepted

Title

Reference Queue Before Lock-Free Queue

Decision

Implement the Scheduler using a reference queue implementation before introducing a lock-free queue.

The reference queue will use standard library synchronization primitives and keep the Scheduler API independent from the queue implementation.

The lock-free queue will replace the reference implementation in a later development phase without changing the Scheduler or Worker interfaces.

Reason

The primary goal of the current phase is to validate the scheduler architecture.

A production-quality lock-free MPMC queue is significantly more complex and would increase the scope of the current PR.

Separating architecture verification from performance optimization keeps each PR small, independently buildable, and fully testable.

This decision supersedes ADR-0013 for the current Scheduler implementation phase.

Alternatives

- Implement the lock-free queue immediately.
- Delay Scheduler implementation until the lock-free queue is complete.

Consequence

Scheduler development proceeds using a reference queue implementation.

Lock-free queue development becomes an independent implementation phase.

Existing Scheduler tests will be reused to verify correctness after replacing the queue implementation.

--

## ADR-0015

Date

2026-06-29

Status

Accepted

Title

Retain Reference Queue for Validation

Decision

Keep the Reference Queue implementation alongside the Lock-Free Queue during development.

The Scheduler will depend on an IQueue interface rather than a concrete queue implementation.

Reason

Maintaining a reference implementation allows:

- Correctness verification
- Regression testing
- Performance benchmarking against a known baseline
- Easier debugging of lock-free behavior

Alternatives

- Replace the Reference Queue immediately after implementing the Lock-Free Queue

Consequence

The project will maintain two queue implementations during development:

- ReferenceQueue
- LockFreeQueue

Production Scheduler code will depend only on the IQueue abstraction.

---

## ADR-0016

Date

2026-06-30

Status

Accepted

Title

Scheduler Queue Abstraction

Decision

Scheduler uses a queue abstraction layer.

The Scheduler and Worker components do not depend on a specific queue implementation.

Queue implementations provide the same interface and can be replaced independently.

Current implementations:

- ReferenceQueue
- LockFreeQueue


Reason

Scheduler architecture needs to be validated independently from queue performance optimization.

A reference implementation allows correctness testing before introducing complex concurrent data structures.

The lock-free queue can be evaluated and replaced without modifying Scheduler or Worker.


Alternatives

- Couple Scheduler directly with LockFreeQueue.
- Delay Scheduler implementation until LockFreeQueue is production-ready.


Consequence

Scheduler development can continue with interchangeable queue backends.

Future queue optimization will not require Scheduler architecture changes.

---

## ADR-0017

Date

2026-06-30

Status

Accepted

Title

ThreadPool Owns Worker Lifecycle

Decision

ThreadPool manages Worker creation, execution, and shutdown.

Scheduler delegates worker management responsibilities to ThreadPool.

Reason

Scheduler should coordinate task execution rather than directly manage worker threads.

Separating responsibilities keeps queue abstraction, worker management, and scheduling logic independent.

Alternatives

- Keep Worker management inside Scheduler.
- Create threads directly inside Scheduler.

Consequence

ThreadPool becomes the owner of:

- Worker instances
- Worker lifecycle
- Task submission routing

Scheduler only controls high-level scheduling operations.

---

## ADR-0018

Date

2026-06-30

Status

Accepted

Title

Stage Parallel Binary Splitting Implementation

Decision

Implement Binary Splitting parallelization in two stages.

Stage 1 introduces a parallel-ready API while preserving the current sequential implementation.

Stage 2 integrates Scheduler synchronization primitives to execute recursive Binary Splitting tasks in parallel.

Reason

The current Scheduler executes fire-and-forget tasks but does not provide task synchronization or result aggregation.

Binary Splitting requires joining recursive computations before merging intermediate results.

Separating API preparation from Scheduler synchronization keeps each PR independently buildable, testable, and easier to review.

Alternatives

- Implement Parallel Merge immediately with temporary synchronization.
- Delay all Binary Splitting changes until Scheduler synchronization is complete.

Consequence

BinarySplitter will expose both sequential and parallel-ready execution paths.

Actual parallel execution will be implemented after Scheduler gains task synchronization support (Future/TaskGroup/Join mechanism).

This keeps the BinarySplitter interface stable while allowing Scheduler improvements to be integrated later.

---

## ADR-0019

Date

2026-07-19

Status

Accepted

Title

Shared Task Completion State and Exception Propagation

Decision

`Task` and its returned `TaskHandle` share one reference-counted task state.

The shared state owns:

- `TaskState`
- captured `std::exception_ptr`
- completion mutex
- completion condition variable

`TaskHandle` observes the shared state and does not own or reference the `Task`
object itself. The handle remains move-only.

`ThreadPool::submit(Task)` and `Scheduler::submit(Task)` return a `TaskHandle`.
Accepted tasks return a valid handle. Rejected submissions return an invalid
handle.

`Task::execute()` captures task exceptions, marks the shared state as failed,
and notifies waiters. Exceptions must not escape the worker thread. Callers
inspect or rethrow captured failures through the handle.

Reason

Binary Splitting requires callers to wait for child tasks before merging their
P/Q/T results. The current fire-and-forget scheduler cannot express this join.

Embedding independent synchronization members in both `Task` and `TaskHandle`
would not provide one observable completion state. Holding a
`std::shared_ptr<Task>` in the handle would also couple task lifetime and result
observation unnecessarily.

Alternatives

- Keep fire-and-forget boolean submission.
- Store a `std::shared_ptr<Task>` in each handle.
- Give `Task` and `TaskHandle` separate atomic states and synchronization data.
- Introduce Binary Splitting-specific synchronization outside the scheduler.

Consequence

Scheduler callers can wait for accepted work and observe completion or failure
without polling.

Worker threads remain alive when an individual task throws.

PR-0016 implements this shared-state synchronization contract without changing
the existing queue topology.

Scheduler shutdown semantics and global/local queue routing are handled in
PR-0017 before parallel Binary Splitting is introduced.

---

## ADR-0020

Date

2026-07-19

Status

Accepted

Title

Layered Checkpoint and Result Integrity Verification

Decision

Checkpoint integrity and mathematical result verification are separate layers.

Every checkpoint block uses a versioned header containing:

- file magic
- format version
- target computation identity
- Chudnovsky range `[a, b)`
- tree level
- P/Q/T payload lengths
- checksum type and checksum value
- completion metadata

Checkpoint files are written to a temporary path, flushed, synchronized, and
atomically renamed only after the complete payload and checksum are available.

Resume accepts only blocks that pass all applicable checks:

1. header and format validation
2. range, level, and payload-size validation
3. payload checksum validation
4. manifest consistency validation
5. independent modular verification of P/Q/T values

Incomplete temporary files and invalid blocks are never treated as completed
work. They are quarantined or ignored and their ranges are scheduled for
recalculation.

Final output verification remains a separate stage using output hashing, known
digits, and BBP spot checks.

Reason

A file checksum detects storage corruption but does not prove that a validly
stored P/Q/T value was computed correctly. Independent modular verification is
required to detect computational errors without repeating full-precision work.

Resume correctness depends on trusting only validated blocks. Persisted
progress counters alone are not sufficient evidence that work is reusable.

Alternatives

- Trust every file present in the checkpoint directory.
- Use only a payload checksum.
- Store only a manifest completion flag.
- Recompute every checkpoint block at startup.

Consequence

PR-0021 defines the versioned P/Q/T block and atomic commit format with checksum
metadata from the beginning.

PR-0022 implements structural, checksum, manifest, and modular verification and
resumes only from validated blocks.

Progress reconstruction uses the set of validated blocks rather than stored
percentage values.

---

## ADR-0021

Date

2026-07-19

Status

Accepted

Title

Decouple Progress Tracking from CLI Reporting

Decision

Computation and scheduler code do not print progress directly.

A thread-safe `ProgressTracker` records raw counters and produces immutable
`ProgressSnapshot` values. A reporter interface consumes snapshots without
owning or controlling computation.

The snapshot model is versioned and can expose:

- current computation phase
- target digits and total terms
- completed terms and blocks
- current merge level
- active and queued tasks
- elapsed time, throughput, and optional ETA
- current memory use
- checkpoint bytes
- last validated checkpoint
- terminal completion or failure state

The initial reporters are:

- human-readable CLI text
- machine-readable CLI JSON

TTY output may update one terminal line. Non-TTY output emits complete records
that remain suitable for logs and automation. Future status-file, GUI, metrics,
or remote reporters implement the same reporter boundary.

Worker threads update lightweight counters only. Formatting and output run on a
dedicated reporting path and must not perform disk I/O on computation workers.

TOML and command-line configuration control whether reporting is enabled, the
reporting interval, and the output format. One shared validation path runs after
defaults, TOML loading, and command-line overrides have been applied.

On resume, progress is reconstructed from checkpoint blocks that passed ADR-0020
integrity validation. A persisted percentage is never treated as authoritative.

Reason

Direct CLI output from algorithms would couple computation to one presentation
environment and make JSON, status files, GUI integration, and automated
monitoring difficult to add later.

Raw counters remain useful even when percentage and ETA estimates change as the
cost of high-level merges grows.

Alternatives

- Print directly from worker tasks.
- Store a single global percentage value.
- Make the CLI query internal scheduler containers directly.
- Persist and trust only the last displayed percentage.

Consequence

PR-0022 introduces the phase model, tracker, snapshot, configuration controls,
and CLI text/JSON reporters.

Additional reporting environments can be added without changing Binary
Splitting, checkpoint, or scheduler implementations.

---

## ADR-0022

Date

2026-07-19

Status

Accepted

Title

Explicit Restartable Scheduler Lifecycle

Decision

`Scheduler` and `ThreadPool` use one observable lifecycle model:

- `Stopped`
- `Running`
- `Stopping`

A newly constructed scheduler is `Stopped`. `start()` moves it to `Running`,
and `stop()` moves it through `Stopping` to `Stopped`. Repeated start or stop
calls are idempotent in their corresponding stable state.

Calls are serialized by a lifecycle mutex. A start that overlaps `Stopping`
waits for that stop operation and then restarts the workers. A stop that
overlaps another stop waits for the same stop generation to complete.

Submission is accepted only while the state is `Running`. Once `Stopping` is
published, new submissions return an invalid `TaskHandle`. `running()` returns
true only for `Running`; callers that need the full distinction use `state()`.

The lifecycle mutex is released before joining workers. This prevents a worker
task attempting submission during shutdown from waiting on a lock held by the
thread that is joining that same worker.

Restart after a completed stop is supported.

Reason

The previous atomic boolean did not distinguish a completed stop from a stop in
progress and did not serialize start, stop, and submission. That allowed races
where work could be accepted after shutdown began or worker threads could be
started and stopped concurrently.

A distinct `Stopping` state creates a clear submission cutoff and gives drain
shutdown a stable lifecycle boundary.

Alternatives

- Keep one atomic running boolean.
- Make Scheduler instances single-use after stop.
- Hold the lifecycle mutex while joining every worker.
- Allow submission during `Stopping`.

Consequence

Scheduler lifecycle transitions are observable and restartable. Concurrent
start, stop, and submit calls have a defined ordering.

The drain guarantee paired with this lifecycle cutoff is defined and completed
by ADR-0023. This decision defines the cutoff; ADR-0023 defines completion of
work accepted before it.

---

## ADR-0023

Date

2026-07-20

Status

Accepted

Title

Outstanding-Task Drain and Context-Aware Queue Routing

Decision

`ThreadPool` maintains an atomic count of accepted tasks that have not finished
execution. The count includes tasks in the global queue, tasks in worker-local
queues, stolen tasks, and tasks currently executing.

The task count is reserved before a task is published to a queue. A rejected or
throwing queue insertion rolls back the reservation. A worker decrements the
count only after `Task::execute()` has published completion or failure.

Shutdown uses two phases:

1. Publish the stop request to every worker.
2. Join every worker after it drains accepted work.

A worker continues local pop, global pop, and steal attempts after receiving a
stop request. It exits only when the accepted outstanding-task count is zero.
Therefore `stop()` returns only after every accepted `TaskHandle` is terminal.

Submission routing depends on the calling context:

- external thread to bounded global `IQueue`
- worker owned by this pool to that worker's local queue
- worker owned by another pool to this pool's global queue

The public API remains `submit(Task) -> TaskHandle`. A full global queue rejects
the external submission with an invalid handle.

Reason

Queue emptiness alone cannot prove that drain is complete. A queue may be empty
while a worker is still executing an accepted task, and separately checking
multiple local queues creates transient snapshots with no single correctness
boundary.

---

## ADR-0030

Date

2026-07-23

Status

Accepted

Title

Define the Canonical Runtime Chunk Index Contract

Decision

`ChunkIndex` maps one `ChunkIdentity` to exactly one durable
`ChunkIndexEntry`. Entries are returned in the canonical `ChunkIdentity` order
defined by computation identity, start, end, and tree level. Adding a duplicate
identity is rejected. Two entries with the same computation identity and tree
level must not have overlapping half-open ranges; ranges at different tree
levels are independent.

Each entry records the plain storage filename, uncompressed and stored sizes,
CRC32C checksum metadata, and chunk format version. Filenames cannot contain
path separators or NUL bytes. The aggregate stored size is computed from index
entries with checked saturation.

The index wire format is versioned, uses explicit big-endian integer fields,
contains the complete computation identity and block location, and ends with
a CRC32C over all preceding bytes. Decode validates the checksum, format,
identity, filename, duplicate, and overlap invariants before publishing an
index. File saves use the existing durable atomic file commit primitive.

Reason

The runtime index is a publication record, not a history log. Allowing
duplicate identities or overlapping chunks would make restart recovery
ambiguous and could feed duplicate mathematical work into a future merge.

Consequence

`ChunkIndex` is now a stable boundary for the next `StorageManager` step.
Connecting it to `ChunkStore` and adding process-restart integration remain
separate implementation work; `VersionedChunkIndex` remains available for
legacy version-history tests and is not the canonical PR-0025 runtime index.

---

## ADR-0031

Date

2026-07-24

Status

Accepted

Title

Use ChunkCodec as the ChunkStore On-Disk Representation

Decision

`ChunkStore::store` accepts a complete runtime `Chunk` and writes exactly the
versioned byte sequence produced by `ChunkCodec::encode`. `load` and
`reloadAndVerify` return decoded `Chunk` values through `ChunkCodec::decode`.
The filesystem contains one `.chunk` file per identity and no separate
metadata sidecar. Atomic publication and directory durability remain the
responsibility of `ChunkStore`; structural, identity, size, CRC32C, compression,
and canonical GMP checks remain the responsibility of `ChunkCodec`.

The storage policy compression must match the chunk metadata compression.
Unsupported compression remains rejected until a codec implementation is
added. A corrupt or incompatible stored file is never returned as a partially
decoded value; the store boundary reports it as an integrity failure.

Reason

Two independent representations allowed the payload and metadata to diverge
and duplicated checksum/format logic. One canonical file format makes restart
reload and future index publication unambiguous.

Consequence

ChunkStore and ChunkCodec now form one tested storage boundary. The next step
is to publish the same successful store operation into `ChunkIndex` and to
implement `StorageManager` around both components.

---

## ADR-0032

Date

2026-07-24

Status

Accepted

Title

Define the Synchronous StorageManager Boundary

Decision

`StorageManager` owns one `ChunkStore` and one durable `ChunkIndex`. Its
`store` operation rejects an indexed duplicate, durably writes the canonical
`ChunkCodec` file, builds a candidate index containing the new entry, and
atomically publishes that index. If index construction or publication fails,
the newly written chunk is removed and the previously published index remains
unchanged.


`load` is index-gated: an unindexed chunk is not loadable through the manager.
The loaded chunk is decoded and checked against the index identity, stored
size, and checksum before being returned. `remove` removes the indexed file
and publishes an index without that identity. `snapshot` reports indexed
chunk count, stored bytes, resident bytes supplied by the caller, and the
configured memory budget. Eviction planning is delegated to the synchronous
`ChunkStore` planner and never mutates resident memory.

All operations are synchronous and the manager does not perform automatic
Binary Splitting eviction. Merge integration and progress reporting remain
later work.

Reason

The manager is the narrow orchestration boundary needed to connect durable
chunk bytes, the canonical index, and deterministic memory planning without
making the arithmetic algorithm aware of filesystem or codec details.

Consequence

Process restart can reconstruct the manager from the durable index and
canonical chunk files. The next integration step is to reconcile missing or
corrupted indexed files explicitly and then expose manager counters through
the progress model.

Counting accepted unfinished work creates one drain invariant across global,
local, stolen, and active tasks.

External submissions must use the global queue so configured capacity and
rejection are observable. Worker-created child work belongs in the local queue
so it remains cheap and available for stealing.

Alternatives

- Stop workers immediately when a stop flag is set.
- Exit when every queue appears empty.
- Route every submission directly to a round-robin worker local queue.
- Expose separate public APIs for external and local submission.
- Join each worker immediately after requesting that individual worker stop.

Consequence

Accepted work cannot remain permanently pending after normal drain shutdown.
Global queue capacity applies to external producers, while nested worker work
uses local queues without changing the scheduler API.

The current idle path still yields while waiting for work or drain completion.
Wake-up optimization remains deferred until benchmarked.

---

## ADR-0024

Date

2026-07-20

Status

Accepted

Title

Bounded MPMC Slot-Sequence Contract

Decision

`LockFreeQueue` remains a bounded multi-producer, multi-consumer queue with one
atomic sequence value per slot and separate producer and consumer position
counters.

Capacity must be at least two and no greater than `PTRDIFF_MAX`. This keeps the
occupied and reusable slot generations distinct and makes modular sequence
ordering unambiguous within the supported queue distance.

Push and pop classify a slot sequence relative to the operation's expected
sequence:

- equal: claim the position with compare-and-exchange
- behind: report full to push or empty to pop
- ahead: reload the shared position because the prior observation is stale

The position counters use relaxed atomic operations because they only allocate
positions. A release store to the slot sequence publishes the task payload, and
an acquire load of that sequence makes the payload visible before it is moved.
The same release/acquire relationship protects reuse after consumption.

`empty()` inspects the next consumer slot sequence. Its answer is only a
concurrent snapshot and cannot replace accepted outstanding-task accounting for
drain shutdown.

Reason

Treating every sequence mismatch as full or empty confuses contention with a
stable queue boundary. Another producer or consumer can advance the shared
position between the initial position load and the slot sequence load, leaving
the observer with a stale position even when the queue can still make progress.

Capacity zero causes invalid slot indexing. With one slot, the initial free
sequence and the post-consumption reusable sequence collide with the occupied
generation required by this protocol.

Alternatives

- Serialize the queue with a mutex.
- Keep retrying every mismatch without distinguishing true full or empty.
- Require a power-of-two capacity and mask slot indices.
- Use reservation-counter equality as the definition of empty.

Consequence

Queue contention no longer creates false rejection solely from a stale
position. Arbitrary capacities of at least two remain supported, and slot
publication has an explicit memory-ordering contract.

Direct tests validate capacity boundaries, FIFO and slot reuse, unused-capacity
producer contention, and exactly-once execution under concurrent producers and
consumers.

---

## ADR-0025

Date

2026-07-20

Status

Accepted

Title

Use a Staged DAG for Parallel Binary Splitting

Decision

Parallel Binary Splitting uses a caller-orchestrated staged DAG rather than
recursive worker tasks that submit child tasks and wait for their handles.

The caller partitions the input into balanced sequential leaf blocks, bounded
by an explicit tasks-per-worker policy. Tasks write exclusively owned result
slots and never wait for other tasks. After joining one complete level, the
caller submits adjacent P/Q/T merges for the next level. Levels with fewer than
two merge pairs execute inline.

The parallel API receives a scheduler reference and an explicit cutoff. It
falls back synchronously for small ranges, stopped or zero-worker schedulers,
calls from a worker owned by that scheduler, and rejected submissions.

Reason

The scheduler's `TaskHandle::wait()` blocks but does not execute queued work.
Recursive fork/join can therefore occupy every worker with a parent waiting on
children that remain in local queues. A staged DAG has no worker-side waits and
makes every dependency boundary explicit.

Bounded independent blocks limit task metadata, expose useful parallelism, and
map naturally to future checkpoint, progress, NUMA, and memory-reclamation
stages. Internal result slots avoid expanding the completion-only task handle
into a generic future before other consumers require that abstraction.

Alternatives

- Submit recursive child tasks and block worker threads on their handles.
- Add cooperative wait-and-help behavior to the scheduler in the same PR.
- Introduce a generic typed future and task graph runtime before parallelizing.
- Use a global scheduler and a hidden fixed cutoff.

Consequence

Parallel execution cannot deadlock from every worker waiting on queued child
work. Sequential and fallback paths retain exact P/Q/T equality, and task count
is bounded by worker count rather than total term count.

The caller synchronizes between merge levels, and current-worker calls do not
parallelize. Cooperative scheduler waits and a generic task DAG remain possible
future extensions if broader workloads justify their complexity with
benchmarks.

---

## ADR-0026

Date

2026-07-20

Status

Accepted

Title

Complete the End-to-End Calculation Before Checkpoint Storage

Decision

Insert an end-to-end Chudnovsky calculation PR before the checkpoint block
foundation. The new PR-0020 defines digit-to-term conversion, guard precision,
integer fixed-point finalization, normalized decimal output, and end-to-end
benchmarks. The previously planned checkpoint, integrity, and progress work
moves to PR-0021, PR-0022, and PR-0023 respectively.

Final pi construction uses guarded GMP integer arithmetic and integer square
root instead of promoting the test-only `mpf_t` approximation into the
production API.

Reason

Checkpoint identity and compatibility depend on the requested digits, guard
policy, term count, arithmetic version, and finalization contract. Defining a
durable format before those fields exist risks format churn and makes it
impossible to benchmark the real end-to-end bottleneck.

Integer fixed-point finalization makes rounding inputs explicit, avoids hidden
global floating-point precision, and preserves the project's GMP integer
boundary for very large calculations.

Alternatives

- Implement checkpoint blocks before the final calculation contract.
- Promote the test-only GMP floating-point calculation directly to production.
- Combine finalization, checkpoint, resume, and progress in one PR.

Consequence

The next implementation target is PR-0020 end-to-end Chudnovsky calculation.
Checkpoint work starts only after computation identity fields and realistic
performance measurements are available. Existing checkpoint architecture
decisions remain valid but their planned PR numbers move forward by one.

---

## ADR-0027

Date

2026-07-20

Status

Accepted

Title

Use Canonical Versioned Checkpoint Bytes and Atomic Synchronous Storage

Decision

Checkpoint compatibility is based on the Chudnovsky algorithm and formula
versions, requested and guarded precision, working digits, and term count.
Runtime worker, cutoff, queue, and timing settings are separate provenance.

P/Q/T values use canonical signed magnitude with minimal big-endian bytes.
Blocks and manifests encode every field explicitly, protect canonical bytes
with CRC32C, and reject non-canonical or inconsistent input. Storage uses a
unique same-directory temp file, complete writes, file synchronization, atomic
rename, and parent-directory synchronization.

Reason

Portable canonical bytes prevent compiler ABI, padding, native endianness, and
equivalent-encoding differences from changing durable checkpoint identity.
Separating codec and synchronous storage permits a future writer thread or
out-of-core layer without changing the file contract or adding disk I/O to
scheduler workers.

Consequence

Equivalent blocks and manifests produce deterministic bytes and paths across
execution policies. Failures before rename preserve the previous final file.
PR-0022 remains responsible for deciding whether persisted blocks are trusted
for resume, including manifest-set consistency and independent modular P/Q/T
verification.

---

## ADR-0028

Date

2026-07-20

Status

Accepted

Title

Verify Canonical Final Output Through Independent Evidence Layers

Decision

Final decimal output is canonical `3.` plus exactly the requested ASCII
fractional digits. Stored files add one LF byte, while SHA-256 covers only the
newline-free canonical bytes.

Every completed CLI computation uses three independent evidence layers:

- a versioned rounded known-decimal prefix;
- canonical SHA-256, persisted in a versioned CRC32C-protected manifest;
- deterministic BBP hexadecimal samples compared with hexadecimal digits
  extracted from the actual decimal output through exact GMP arithmetic.

BBP calculation retains a numeric error bound and returns inconclusive at a
digit boundary. Decimal extraction includes the complete half-decimal-ULP
rounding interval. A skipped inapplicable check is neutral when other required
checks pass, but an all-skipped report remains skipped.

Final verification manifests are published only for passed reports through
durable same-directory atomic replacement. Later file validation compares the
stored canonical SHA-256 so arbitrary middle and trailing digit mutation is
detectable independently of sampled mathematical checks.

Reason

Hashing, known digits, and BBP detect different failure classes. BBP must check
the generated output rather than only another embedded reference to remain an
independent mathematical cross-check. Canonical bytes and a versioned manifest
make verification portable and reproducible outside the calculation process.

Consequence

Progress schema version 2 includes `verifying_output`. The CLI owns output,
verification, manifest publication, and terminal progress after the calculator
finishes arithmetic. Default verification uses eight deterministic samples;
future large-scale latency work should parallelize independent BBP samples
without creating threads inside the algorithm.

---

## ADR-0029

Date

2026-07-21

Status

Accepted

Title

Define the Runtime Storage Lifecycle and Ownership Boundary

Decision

PR-0025 runtime storage uses the following lifecycle for a chunk record:

- `resident`: the authoritative `BinaryNode` value is available in memory and
  has not been committed as a runtime-storage result.
- `spilling`: a synchronous store operation is serializing and committing the
  resident value. The original resident value remains available and is not
  released during this state.
- `stored`: the complete chunk file and its index entry have been committed
  durably and the chunk can be loaded independently of the current process
  memory state.
- `loading`: a stored chunk is being read, structurally validated, checksum
  verified, and decoded. No partially decoded value is published to callers.
- `corrupted`: a discovered stored artifact failed format, identity, range,
  checksum, or canonical GMP validation. It is not eligible for merge or
  reload until removed and recomputed.
- `removed`: the runtime-storage record has no usable durable chunk. Loading
  it is an explicit not-found failure.

Valid transitions are:

```text
resident -> spilling -> stored
resident <- spilling       (store failure; resident value is preserved)
stored   -> loading -> resident
stored   -> corrupted
corrupted -> removed
stored   -> removed
```

`loading` failure transitions to `corrupted` for an existing but invalid
artifact, and to `removed` when the artifact is absent. A failed load never
replaces an existing resident value. `stored` means that the durable copy is
available; PR-0025 does not automatically evict the caller's in-memory value.
Resident-byte accounting and eviction planning are separate concerns from the
durable chunk state.

The caller owns the in-memory `BinaryNode` and remains responsible for its
lifetime. `StorageManager` owns chunk metadata, the chunk index, storage
paths, and codec/backend selection. Storage APIs accept or return values at a
chunk boundary and never expose file paths or filesystem details to Binary
Splitting. A successful `store` publishes a chunk only after the complete
payload, checksum, file synchronization, atomic rename, and index update have
completed. A failed store leaves the original resident value and previous
valid index entry unchanged; an unindexed orphan file is never loadable.

Runtime storage is distinct from checkpoint storage. Checkpoints are durable
resume evidence validated by the checkpoint validation pipeline. Runtime
chunks are temporary working data used for memory pressure and future merge
reclamation. Runtime chunks may reuse the canonical P/Q/T codec and CRC32C,
but their presence alone never makes a checkpoint resumable.

All PR-0025 storage operations are synchronous and are called outside
scheduler worker tasks. The lifecycle contract is independent of the future
asynchronous writer, actual Binary Splitting eviction, compression backend,
and merge-wait integration planned for later PRs.

Reason

Out-of-core storage must not turn a failed disk operation into data loss or
allow partially written data to enter a merge. Separating durable runtime
state from resident memory ownership also permits deterministic eviction and
future asynchronous I/O without changing Binary Splitting or checkpoint
compatibility rules.

Alternatives

- Release the resident value before a store completes.
- Treat any file at a deterministic path as loadable.
- Reuse checkpoint completion state for temporary runtime chunks.
- Let Binary Splitting manage paths and codec details directly.

Consequence

PR-0025 can implement `StorageManager` and its synchronous local backend
without changing the Binary Splitting algorithm. The next storage steps must
preserve the original resident value across store failure, publish only fully
committed and indexed chunks, and reject corrupted chunks before decode results
are returned. Actual node eviction and merge integration remain separate PR
boundaries.

---

## ADR-0033

Date

2026-07-24

Status

Accepted

Title

Define Deterministic Memory Budget and Eviction Planning

Decision

Resident bytes are the sum of caller-supplied uncompressed sizes and stored
bytes are the sum of durable encoded file sizes. Both counters saturate at
`uint64_t` maximum instead of wrapping. Subtraction saturates at zero and
available budget is zero whenever resident bytes meet or exceed the budget.

Eviction candidates with merge distance zero are protected. Negative,
infinite, or NaN merge distances and duplicate resident IDs are rejected.
Candidates are ordered deterministically by descending merge distance,
descending uncompressed size, then ascending chunk ID. The planner selects
whole chunks until the requested resident bytes are met; it never performs a
partial chunk eviction. If the candidates cannot provide enough bytes, the
plan is explicitly unsatisfied and includes a reason.

Reason

Eviction decisions must be reproducible for the same resident set and merge
state. Protecting immediately needed chunks avoids invalidating a future
merge, while whole-chunk selection keeps accounting and state transitions
unambiguous.

Consequence

The planner is ready for synchronous `StorageManager` callers. It still only
produces a plan; actual Binary Splitting node release and merge wait behavior
remain outside PR-0025.

---

## ADR-0034

Date

2026-07-24

Status

Accepted

Title

Treat Large-Chunk Reload as the Current Performance Investigation Target

Decision

The initial synthetic benchmark uses one uncompressed P/Q/T chunk of roughly
100 MiB and includes synchronous durable publication, index update, CRC32C,
and GMP reconstruction. The observed baseline on the current runner was
163.54 MiB/s for store and 11.08 MiB/s for reload. Store throughput is an
initial baseline only; reload is the primary investigation target because it
is roughly one order of magnitude slower.

These numbers are not treated as raw filesystem throughput. The reload value
includes file read, length and checksum validation, canonical payload parsing,
and three GMP integer reconstructions. The next benchmark must split those
phases and compare cold-cache and warm-cache runs before any codec or I/O
optimization is selected.

Follow-up measurements must also cover multiple chunks, concurrent I/O,
compression comparisons, peak RSS, and sanitizer runs. A single large chunk
does not establish scalability or prove that the configured I/O concurrency
limit is effective.

Reason

The current result proves correctness at the requested data scale, but it
cannot distinguish storage-device limitations from CPU/GMP decode cost. The
explicit follow-up matrix prevents optimizing the wrong layer and records the
memory behavior needed for the out-of-core boundary.

Consequence

PR-0025 keeps the current synchronous correctness path unchanged. Profiling,
cache-control, multi-chunk concurrency, compression comparison, and RSS/
sanitizer validation remain tracked follow-up work before performance claims
are generalized beyond this baseline.

---

## ADR-0035

Date

2026-07-24

Status

Accepted

Title

Define the PR-0026 BinaryNode Storage Lifecycle

Decision

The storage boundary uses six states: `resident`, `spilling`, `stored`,
`loading`, `corrupted`, and `removed`. The legal transitions are:

```text
resident -> spilling -> stored
resident <- spilling                 (spill failure; original is retained)
stored -> loading -> resident
loading -> corrupted                 (artifact exists but validation fails)
loading -> removed                   (artifact is absent)
stored -> removed
corrupted -> removed
```

The lifecycle object tracks state but owns neither `BinaryNode` memory nor
filesystem resources. A caller must not release the resident node until the
spill reaches `stored`. A failed spill returns to `resident`; a successful
load returns to `resident`; a failed load never returns an unvalidated value.
`corrupted` artifacts are not loadable again until explicitly removed and
recomputed.

Reason

The merge coordinator needs an explicit ownership and failure contract before
it can connect eviction decisions to StorageManager calls. Keeping the state
machine independent from I/O makes synchronous behavior testable and leaves
room for a future asynchronous writer without changing the state semantics.

Consequence

The next integration step may attach a lifecycle record to each resident node
and apply the transitions around `StorageManager::store()` and
`StorageManager::load()`. Actual node release and merge scheduling remain
outside this state machine.

---

## ADR-0036

Date

2026-07-24

Status

Accepted

Title

Connect Merge-Level Observation to StorageManager Accounting

Decision

`BinarySplitter` exposes an optional `BinaryMergeCoordinator` callback on the
explicit parallel split entry point. The callback observes the immutable
resident node span after leaf construction and after each merge level. The
concrete `StorageMergeCoordinator` converts each node's computation identity,
range, and tree level into a deterministic chunk identity, calculates its
uncompressed resident size, and publishes the current resident set through
`StorageManager::snapshot()`.

This first connection is observation-only: it does not store or release
nodes. That preserves the existing in-memory merge result while establishing
the exact resident-set boundary required by the next spill implementation.
Duplicate identities and invalid node ranges are rejected before accounting
is published.

Reason

The merge coordinator must see the same level boundaries as BinarySplitter,
but the algorithm must not know filesystem or codec details. An optional
abstract callback keeps existing callers unchanged and permits the concrete
storage bridge to depend on StorageManager without forcing every BinaryNode
consumer to link the storage backend.

Consequence

The next step can attach `NodeLifecycle` records and invoke synchronous
`StorageManager::store()` only after a successful observation/planning step.
No current merge path releases resident memory or changes mathematical
results.

---

## ADR-0037

Date

2026-07-24

Status

Accepted

Title

Publish Merge Storage State Through ProgressTracker

Decision

`StorageMergeCoordinator` optionally receives a caller-owned
`ProgressTracker`. At each observed merge level and after spill/reload work it
publishes resident bytes, durable stored bytes, indexed chunk count, and the
current merge level. The tracker remains optional so standalone BinarySplitter
and storage tests do not need a progress lifecycle.

Reason

Storage accounting is useful only when it is sampled at the same boundaries as
the merge coordinator. Publishing from the coordinator avoids making workers
format progress or access filesystem details, and keeps the existing progress
reporters unchanged.

Consequence

Progress output now reflects the synchronous spill/reload boundary. The
calculator integration still needs to construct the coordinator from runtime
configuration and pass the caller-owned tracker in the production path.

---

## ADR-0038

Date

2026-07-24

Status

Accepted

Title

Split Asynchronous and Platform Optimization Work After PR-0026

Decision

PR-0026 is stabilized around a synchronous correctness path: BinarySplitter
merge boundaries, deterministic memory planning, synchronous store/reload,
node lifecycle transitions, corruption rejection, progress publication, and
forced out-of-core regression/performance tests. These boundaries are the
compatibility surface for subsequent storage work.

The following work is deferred to separate PRs:

- PR-0027: asynchronous storage writer/reader, merge wait/backpressure,
  cancellation, progress telemetry, failure handling, and baseline
  performance validation.
- PR-0028: concurrent I/O scheduling, compression backend
  optimization/benchmark selection, and NUMA/Huge Pages placement.

Neither deferred PR may change chunk identity, canonical codec bytes, index
durability, lifecycle failure semantics, or the synchronous correctness path
without an explicit contract revision and regression coverage.

Reason

Combining asynchronous scheduling and platform-specific memory optimization
with the first real spill integration would make failures, ownership, and
performance regressions difficult to isolate. The current synchronous path
provides a stable reference implementation for measuring those later changes.

Consequence

PR-0026 can be reviewed and released as a correctness/stability milestone.
The first task of PR-0027 is to construct `StorageManager` and
`StorageMergeCoordinator` from the validated runtime configuration and inject
the caller-owned `ProgressTracker` into the production calculator path. Only
after that wiring is covered by regression tests should PR-0027 begin the
asynchronous writer contract, preserving the existing store-before-release
rule and lifecycle states.

The production wiring portion is now implemented and covered by calculator
build and regression execution. The bounded writer, merge integration, and
reload reader are tracked as the remaining PR-0027 asynchronous work.

---

## ADR-0039

Date

2026-07-24

Status

Accepted

Title

Define the Asynchronous Chunk Writer Contract

Decision

One asynchronous write request follows this lifecycle:

```text
queued -> writing -> stored
queued -> cancelled
queued -> failed
writing -> failed
```

Only `stored`, reached after complete payload write, file synchronization,
atomic rename, and index publication, establishes a durable copy. The caller
must retain the resident BinaryNode while a request is `queued` or `writing`.
Queued requests may be cancelled; active writes drain or fail so cancellation
cannot report success before durability is known. Failed requests carry owned
diagnostic text and never imply a durable copy.

The initial contract is represented by `AsyncWriteLifecycle`. It contains no
thread, queue, filesystem, or payload ownership so the later bounded writer
can implement scheduling without changing lifecycle semantics.

Reason

Asynchronous I/O must preserve the synchronous store-before-release guarantee.
Making durability and cancellation states explicit prevents a future writer
from releasing resident memory when a request was merely accepted by a queue.

Consequence

The next implementation step is a bounded request queue and writer runner
that drives this lifecycle and propagates completion/failure to the merge
coordinator and progress telemetry.

The bounded queue and writer runner are now implemented and connected to the
merge coordinator. Asynchronous reload is covered by the corresponding reader
and merge preparation integration; concurrent I/O scheduling remains a later
PR-0028 item.

NodeLifecycle now consumes the writer completion state: `stored` transitions
to durable storage, while `failed` and `cancelled` return to `resident` with
the original value preserved.

The merge coordinator now submits spill candidates to the bounded writer,
waits for capacity before submission, waits for durable completion handles,
and clears node payloads only after successful completion. Queue shutdown or
write failure leaves affected nodes resident and propagates the failure.

The same boundary now exists for reload. `AsyncChunkReader` owns a bounded
request queue and worker lifecycle; each request calls the validated
`StorageManager::load` path, and `prepareMergeNodes` waits for every queued
handle before decoding and restoring P/Q/T. A failed request never exposes a
partially initialized BinaryNode. The synchronous reload path remains the
fallback when no reader is injected; production out-of-core construction now
owns both async workers and injects them into the merge coordinator using the
configured maximum-concurrent-I/O value.

Failure and shutdown regression coverage verifies duplicate publication
failure, missing indexed reload failure, terminal counters, and rejection of
new requests after worker shutdown.

The Release benchmark now accepts `out-of-core async` to compare the bounded
writer/reader path with the synchronous coordinator. A current 1,000,000-digit
forced-1-MiB run measured 0.512 s sync versus 0.544 s async, with identical
15-spill/15-reload counts and correct P-bit output. Async peak RSS was 27 MiB
versus 23 MiB sync. The small regression is expected for the current
single-worker queue and remains a tuning baseline until concurrent I/O and
larger multi-chunk workloads are measured.

Progress reports the asynchronous storage boundary separately from scheduler
task counts. Writer and reader queue depth, active operations, completed
operations, and failures are published through `ProgressTracker` and exposed
by both progress reporters; storage byte and chunk telemetry remains
unchanged.

PR-0029 measurement criteria are fixed: Release configuration and host
metadata are recorded; 100 MiB/512 MiB/1 GiB single- and multi-chunk workloads
are tested in separate cold- and warm-cache populations; sync/async modes,
worker counts 1/2/4/8, and queue capacities 1/4/16/64 are compared; and each
warm case has one untimed warm-up plus at least five measured repetitions.
Results must include stage timings, throughput, p50/p95, peak RSS, queue and
failure telemetry, spill/reload counts, raw samples, and P/Q/T equality before
an optimization is accepted.

PR-0028 concurrent I/O now permits distinct chunk file reads and writes to
overlap across async workers. StorageManager serializes only index publication
and protects in-flight identities; the worker-wide manager mutex was removed.
The 1,000,000-digit forced-1-MiB benchmark measured 0.549 s with one async
worker and 0.423 s with four workers, with equal spill/reload counts and P-bit
results. This is an initial result; the full workload matrix remains required.

Async spill submission now transfers the newly encoded Chunk payload into the
writer operation by move, while capturing stored-size metadata before the
transfer. This removes one queue-entry GMP payload copy without releasing the
resident BinaryNode before durable completion. Buffer reuse and reload-side
payload movement remain measurement-gated follow-up work.

The runtime compression contract now supports `none` and LZ4. LZ4 output is
bounded by the existing chunk payload limit, decompression requires the exact
declared uncompressed size, and canonical chunk identity remains
compression-independent. Durable round-trip and CRC validation are covered;
full none-vs-LZ4 performance claims remain benchmark-gated.

The current platform audit found one NUMA node on an Intel Core Ultra 9 185H
runner and no configured explicit HugeTLB pages (`HugePages_Total=0`). No NUMA
or Huge Pages code is enabled from this evidence; affinity remains opt-in
because hybrid-core placement can regress storage and GMP workloads. A
multi-node or explicitly provisioned HugeTLB host must provide a measured
benefit before platform-specific placement is accepted.

The throughput benchmark now accepts a `none` or `lz4` compression argument.
On the current runner for one 100 MiB target chunk, none encoded 99.66 MiB
with 3.27 s total time, while LZ4 encoded 70.05 MiB with 2.74 s total time.
The measured store/reload rates were 83.60/47.94 MiB/s for none and
66.75/41.49 MiB/s for LZ4 when expressed over encoded bytes. These values are
baseline evidence only; workload matrix and cold/warm cache measurements are
still required before choosing a default.

---

## ADR-0040

Date

2026-07-24

Status

Accepted

Title

Separate PR-0028 Functional Storage Work from PR-0029 Measurement Work

Decision

PR-0028 owns the functional concurrent storage pipeline: concurrent
StorageManager file operations, async writer/reader execution, spill data
movement, and the bounded LZ4 backend. PR-0029 owns repeatable workload
measurement, stage-level telemetry expansion, bottleneck optimization, and
large-data/platform acceptance.

Reason

The functional path must first be verified against the PR-0027 correctness
reference. Mixing implementation changes with cache-controlled repetitions,
profiling, and platform-specific tuning would make correctness regressions and
performance claims difficult to attribute.

Consequence

PR-0028 is functionally verified by the full 64-test suite, async multi-chunk
round-trip benchmark, and 512 MiB none/LZ4 round trips. Its remaining work is
change separation, final documentation, commit, and push. PR-0029 begins only
after that boundary is committed and uses the PR-0028 result as its baseline.

---

## ADR-0041

Date

2026-07-24

Status

Accepted

Title

Fix the PR-0029 Measurement Environment and Acceptance Contract

Decision

PR-0029 comparisons use fixed Release/compiler/CPU/filesystem populations,
separate cold and warm cache samples, one untimed warm-up for warm samples,
and at least five measured repetitions per workload cell. Results must retain
raw samples and report median, minimum, maximum, p95, throughput, peak RSS,
queue telemetry, integrity status, and P/Q/T equality.

The current `/tmp` tmpfs benchmark population is valid for a logical
CPU/codec/storage-pipeline baseline but is not interchangeable with the
workspace ext4 population. Device-I/O claims require an explicit ext4
benchmark directory and a separately labeled result set.

Reason

The previous single-run results mixed cache and filesystem effects and did not
provide enough variance data to approve an optimization. Separating the
populations prevents a tmpfs result from being misreported as durable-device
throughput.

Consequence

The next PR-0029 step is to add or select benchmark directory control and
stage-level telemetry, then execute the fixed matrix. No optimization is
accepted until it passes the correctness, durability, memory, measurement,
and portability gates in the canonical PR-0029 sequence in
`docs/IMPLEMENTATION_PLAN.md`.

---

## ADR-0042

Date

2026-07-25

Status

Accepted

Title

Keep PR-0029 Telemetry as Optional Aggregate Instrumentation

Decision

Storage stage timing is accumulated through an optional caller-owned
`StorageTiming` object. Async reader and writer queue wait and active durations
are exposed as aggregate counters. Timing fields are observations only and do
not participate in chunk identity, codec bytes, lifecycle state, or progress
correctness.

Reason

Telemetry must be available to benchmarks without imposing a timing object on
normal production callers or changing storage semantics. Aggregate counters
are safe for concurrent workers and sufficient for the first workload matrix.

Consequence

The next PR-0029 step is explicit storage-directory control and repeated
cold/warm matrix execution. Per-request traces, buffer allocation tracing, and
optimization remain deferred until aggregate telemetry identifies a target.

---

## ADR-0043

Date

2026-07-25

Status

Accepted

Title

Decode Once During Verified Reload

Decision

`ChunkStore::reloadAndVerify` reads and decodes a chunk once. The codec decode
performs structural, size, checksum, compression, and GMP validation; the
reload boundary additionally checks the deterministic filename identity and
active compression policy. The standalone `verifyChunkIntegrity` operation
remains available for audit scans.

Reason

The previous reload path verified the complete file and then loaded and
decoded it again, doubling file-read, CRC, decompression, and GMP decode work
for every reload without adding a distinct validation guarantee.

Consequence

Reload integrity semantics are preserved while normal reload CPU and I/O are
reduced. The benchmark telemetry now represents one decode per reload; audit
scans still use the explicit verification API.

---

## ADR-0044

Date

2026-07-25

Status

Accepted

Title

Defer Platform-Specific Storage Optimization

Decision

Do not add NUMA placement, explicit HugeTLB allocation, or THP tuning to the
storage path for the current target host. CPU affinity may be used to create a
reproducible benchmark population, but is not a production optimization until
it demonstrates a sustained improvement. Prioritize filesystem durability
and index commit batching instead.

Reason

The host exposes one NUMA node and no explicit HugeTLB pages. The measured
ext4 workload is dominated by file/directory synchronization and serialized
index publication, so memory locality and page-size changes cannot address
the observed wall-clock bottleneck.

Consequence

Platform work remains a measurement gate rather than a code change. A future
multi-socket/HugeTLB-capable host can reopen this decision with comparative
benchmarks; the current PR-0029 implementation proceeds with filesystem and
commit-path optimization.

---

## ADR-0045

Date

2026-07-25

Status

Accepted

Title

Bounded Chunk Encode Output

Decision

Chunk encoding allocates the final output buffer from the stored-size
metadata and compresses directly into its payload tail. The uncompressed
codec copies serialized P/Q/T directly into that tail; LZ4 writes directly
into the bounded tail buffer. Temporary compressed vectors and the final
payload append are removed from the store hot path.

Reason

The previous path materialized a canonical payload, a separate compressed
vector, and then copied the compressed vector into the final chunk output.
This increased peak memory and data movement without changing the durable
format.

Consequence

Canonical bytes, CRC, metadata sizes, and compression behavior remain
unchanged while one intermediate output allocation and one payload copy are
removed. The optimization is accepted for correctness; sustained throughput
improvement still requires repeated benchmark comparison.

---

## ADR-0046

Date

2026-07-25

Status

Accepted

Title

CRC, Read, and GMP Hot-Path Optimization

Decision

Use an SSE4.2 CRC32C implementation when available, retain the table-based
portable fallback, read chunk files into a size-reserved byte vector using
`file_size` and one binary read, and export non-negative GMP magnitudes
directly without making an unnecessary absolute-value copy.

Reason

Large single-chunk telemetry showed CRC32C, file read, and GMP encode/decode
as the actionable CPU path after bounded output encoding. These changes reduce
loop overhead, vector growth/copying, and positive-value GMP limb copying
without changing the canonical bytes or validation contract.

Consequence

Runtime CPU feature detection keeps unsupported x86 hosts and non-x86 builds
on the portable CRC path. File-size/read failures remain explicit errors, and
negative GMP values still use a temporary absolute value because their sign
cannot be exported as a magnitude in place. Repeated before/after acceptance
is still required before claiming a stable percentage improvement.

---

## ADR-0047

Date

2026-07-26

Status

Accepted

Title

Accept Combined Storage Hot-Path Effect

Decision

Accept the bounded output, CRC/read, and GMP changes as a combined end-to-end
workload improvement. The preserved 12-cell Release before/after population
shows store-plus-load p50 improvement in every cell, with correctness and
canonical bytes preserved. Do not publish separate CRC, read, or GMP
percentage attribution because the PR-0028 baseline predates stage telemetry.

Reason

The baseline and current executables provide comparable store/load timings and
the same encoded bytes, but only the current executable reports the detailed
stage counters. A combined result is supported; a per-stage causal claim is
not.

Consequence

The two repeated before/after acceptance gates are closed at the end-to-end
level. Peak RSS distribution, independent Reader/Writer repetition, and
multi-chunk cold/warm acceptance remain separate PR-0029 gates.
---

## ADR-0048

Date

2026-07-26

Status

Accepted

Title

Accept Single-Chunk Peak RSS Distribution

Decision

Accept the single-chunk peak RSS gate for PR-0029. The preserved current
Release population covers 12 cells and five repetitions per cell. All 60
round-trips succeeded, and every cell's p95 is below the corresponding
recorded PR-0028 release baseline range.

Evidence

The current per-cell p95 values are 491 MiB for 100 MiB/LZ4, 521 MiB for
100 MiB/none, 2,452 MiB for 512 MiB/LZ4, 2,602 MiB for 512 MiB/none,
4,852 MiB for 1 GiB/LZ4, and 5,152 MiB for 1 GiB/none. Filesystem does not
change the acceptance result. The raw population is preserved at
`/tmp/givemepi-pr0029-before-after-complete-after.tsv`.

Consequence

Peak RSS is no longer an open PR-0029 gate. Independent Reader/Writer
repetition and multi-chunk concurrent I/O cold/warm acceptance remain open.

---

## ADR-0049

Date

2026-07-26

Status

Accepted

Title

Accept Independent Reader/Writer Matrix Execution

Decision

Accept the independent Reader/Writer matrix execution and correctness result.
The matrix covers 64 cells and 320 repetitions across workers 1/2/4/8,
queue capacities 1/4/16/64, tmpfs/ext4, and none/LZ4.

Evidence

All 320 runs completed with eight successful writer requests, eight successful
reader requests, and 16 indexed chunks. The aggregate elapsed-time p50 was
0.013 seconds and p95 was 0.215 seconds. Raw samples are preserved at
`/tmp/givemepi-pr0029-reader-writer-matrix.tsv`.

Limitation

The current benchmark does not emit peak RSS, so this decision accepts matrix
execution and correctness only. A separate memory measurement is required if
the final PR-0029 matrix gate requires per-cell RSS.

---

## ADR-0050

Date

2026-07-26

Status

Accepted

Title

Accept Multi-Chunk Concurrent I/O Cold/Warm Matrix

Decision

Accept the multi-chunk concurrent I/O matrix for correctness, lifecycle,
durability, cache-mode, and bounded-memory evidence.

Evidence

The matrix covers sync/async × workers 1/2/4/8 × queue capacities 1/4/16/64
× tmpfs/ext4 × none/LZ4 × cold/warm, five repetitions per cell: 256 cells
and 1,280 runs. All runs succeeded with 15 spills, 15 reloads, and the
expected P-bit result. Aggregate elapsed p50/p95 was 0.368/2.124 seconds;
peak RSS p95 was 27 MiB. No cold/warm correctness regression was observed.
Raw samples are preserved at
`/tmp/givemepi-pr0029-multichunk-concurrent-cold-warm-accepted.tsv`.

Consequence

The multi-chunk concurrent I/O and cold/warm gate is closed. PR-0029 now
only requires final stabilization, regression/sanitizer reruns, and the
final performance decision before commit.

---

## ADR-0051

Date

2026-07-26

Status

Accepted

Title

Freeze PR-0029 After Final Stabilization

Decision

Freeze PR-0029 as complete. All defined workload, correctness, memory,
concurrency, cold/warm, regression, and sanitizer-smoke gates passed. No
additional optimization is added to this PR.

Evidence

The Debug build passed all 64 CTest tests. Release benchmark targets rebuilt
successfully. ASan/UBSan out-of-core smoke passed for sync and async
1,000,000-digit workloads with leak detection disabled. The full raw
populations and acceptance decisions are recorded in the canonical changelog
and referenced benchmark files.

Deferred scope

NUMA, Huge Pages, and affinity remain deferred because the current host has
one NUMA node and no HugeTLB pool. They require a separate PR and suitable
hardware evidence.

Consequence

PR-0029 is ready for commit. Further work belongs to the next PR.
