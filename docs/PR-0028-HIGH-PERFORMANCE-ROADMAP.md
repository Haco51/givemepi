# PR-0028 High-Performance Storage Roadmap

PR-0027 is the correctness and bounded-I/O baseline. PR-0028 may optimize the
implementation only after preserving chunk identity, canonical codec bytes,
NodeLifecycle durability, progress semantics, and the synchronous fallback.

This roadmap defines the PR-0028 implementation contracts and optimization
boundaries. The repeatable workload matrix, bottleneck measurements, and
large-data validation are tracked as execution work in PR-0029 after the
PR-0028 implementation is committed.

## Phase 0: Measurement contract

- Pin compiler, build type, CPU topology, storage filesystem, and benchmark
  dataset.
- Use Release builds with the exact compiler version and record the full CMake
  configuration.
- Use 100 MiB, 512 MiB, and 1 GiB payloads; single- and multi-chunk layouts;
  sync and async I/O; worker counts 1/2/4/8; and queue capacities 1/4/16/64.
- Run cold-cache cases from a fresh temporary storage directory and warm-cache
  cases after one untimed warm-up. Do not mix the two populations.
- Collect at least 5 measured repetitions after warm-up and report median,
  minimum, maximum, and p95 where request samples are available.
- Measure total wall time plus split, backpressure wait, file read/write,
  CRC32C, codec, and GMP decode as independent timings.
- Record effective MiB/s, p50/p95 latency, peak RSS, resident/stored bytes,
  queue depth, active I/O, completed/failed requests, and spill/reload counts.
- Record hardware model, CPU count, RAM, filesystem/device, kernel, compiler,
  git commit, and benchmark command line.
- Compare every optimized result with the synchronous P/Q/T reference and
  reject output mismatch, lifecycle violation, or memory-budget breach.

Exit condition: a repeatable baseline exists for 100 MiB, 512 MiB, and 1 GiB
workloads with single- and multi-chunk layouts.

### Baseline result format

Every result must include:

```text
commit workload_bytes layout cache mode workers queue repetitions
store_seconds reload_seconds total_seconds
store_mib_per_second reload_mib_per_second
p50_ms p95_ms peak_rss_mib resident_bytes stored_bytes
spill_count reload_count queued_peak active_peak failed_count result_match
```

Raw samples and summary statistics must be stored together. A single elapsed
time sample is diagnostic evidence, not an optimization decision.

## Phase 1: Concurrent I/O

- Add independent reader and writer worker counts rather than coupling them.
- Benchmark workers 1/2/4/8 and queue capacities 1/4/16/64.
- Separate file I/O from CRC32C and GMP decode where possible.
- Track backpressure wait time and prevent unbounded resident memory.
- Test overlapping read/write workloads and multi-chunk reload ordering.

Exit condition: concurrency improves throughput without changing output,
durability, lifecycle state, or memory-budget guarantees.

## Phase 2: Data movement and codec efficiency

- Reuse bounded buffers and avoid redundant Chunk/P/Q/T copies.
- Evaluate direct I/O, buffered I/O, preallocation, and batched index commits
  only with filesystem-specific measurements.
- Compare `none` and LZ4 by compression ratio, CPU time, store throughput, and
  reload throughput.
- Keep compression selection outside the canonical identity domain.

Exit condition: the selected path wins on the target workload while retaining
corruption detection and restart compatibility.

## Phase 3: Memory and platform placement

- Measure allocator pressure, resident payload lifetime, and peak RSS.
- Evaluate huge pages only for large GMP arenas and only when TLB behavior
  improves in measurements.
- Evaluate NUMA placement, worker affinity, and storage-device locality on the
  target machine.
- Add ASan/UBSan and large-data restart/integrity runs before acceptance.

Exit condition: platform tuning has a measured benefit and no regression in
memory budget, portability, or failure recovery.

Current platform audit: the development runner has one NUMA node and no
configured explicit Huge Pages, so neither NUMA placement nor HugeTLB should
be enabled by default. CPU affinity remains opt-in and benchmark-gated,
particularly on hybrid-core CPUs.

## Current bottlenecks and planned fixes

The stage-timing prototype used to produce the observations below is a
PR-0029 measurement artifact and is intentionally not part of the PR-0028
functional change set. It must be reintroduced after the PR-0028 baseline is
committed.

1. CRC32C calculation
   - Observation: CRC32C is the largest single load-stage cost in the 100 MiB
     and 512 MiB measurements.
   - Plan: profile buffer access and SIMD paths, then evaluate overlapping
     checksum work with the read pipeline. Preserve the integrity contract and
     compare checksum results byte-for-byte before and after optimization.

2. File reads and buffer copies
   - Observation: file reads are the next major load cost. Large workloads keep
     original P/Q/T values, encoded data, and decoded data alive concurrently,
     increasing peak RSS.
   - Plan: introduce bounded-buffer reuse, streaming encode/decode, and shorter
     read-buffer lifetimes. Consider mmap, prefetch, or io_uring only when a
     benchmark proves a benefit over the baseline.

3. GMP serialization and decode
   - Observation: GMP decode remains a significant load-stage cost.
   - Plan: measure temporary allocations and payload copies, then add reusable
     buffers and move boundaries while preserving canonical GMP bytes and P/Q/T
     equality.

4. Store encode and publication overhead
   - Observation: store elapsed time includes header construction, checksum,
     and index publication in addition to GMP encode, compression, and file
     durability. These costs are not yet independently timed.
   - Plan: split header, checksum, and index publication timings before deciding
     whether batched index commits or preallocation are worthwhile.

5. Async queue and multi-chunk behavior
   - Observation: current multi-chunk results primarily report elapsed time and
     spill/reload counts; queue wait and per-worker read/write stages are not
     sufficiently separated.
   - Plan: add worker-level file-read/write, queue-wait, backpressure, and index
     commit telemetry, then repeat the worker 1/2/4/8 and queue 1/4/16/64 matrix
     at least five times.

6. Large-data stability
   - Observation: 512 MiB round-trips pass, but 1 GiB Release and large
     sanitizer workloads do not complete reliably because of simultaneous
     buffer retention and sanitizer overhead.
   - Plan: shorten buffer lifetimes and add streaming before re-running 1 GiB
     Release. Run the ASan/UBSan gate on a dedicated low-memory workload or
     suitable machine, including partial-file cleanup, restart, CRC, and sync/
     async P/Q/T equivalence checks.

Implementation order:

`stage telemetry → buffer reuse/lifetime → CRC/read optimization → GMP decode
optimization → async queue matrix → large-data revalidation`.

Each stage must pass the existing test suite and Release 100/512 MiB plus
multi-chunk correctness checks before the next optimization is accepted.

## Acceptance gates

- No change to PR-0027 chunk identity or canonical encoded bytes.
- Sync and async output remain bit-for-bit equivalent.
- All existing tests pass under GCC and Clang.
- Large-data sanitizer and restart/integrity tests pass.
- Performance claims include workload, cache state, hardware, compiler, and
  variance information.
