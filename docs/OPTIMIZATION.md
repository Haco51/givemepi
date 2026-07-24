# Optimization Rules

Optimization priority

1 Correctness

2 Algorithm

3 Cache

4 Memory

5 Parallelism

6 SIMD

7 Assembly

Never optimize before profiling.

Always benchmark.

Never replace GMP multiplication.

Never allocate inside hot loops.

Prefer contiguous memory.

Avoid false sharing.

Use thread affinity when benchmark proves improvement.

Use Huge Pages only if beneficial.

Compression only for Out-of-Core Layer.

For PR-0029 storage work, establish a reproducible measurement contract
before changing queueing, buffering, compression, NUMA placement, or page
size. Report cold/warm cache, worker count, queue capacity, p50/p95 latency,
throughput, peak RSS, and separate file/CRC/codec/GMP costs.

The staged roadmap and acceptance gates are in
`docs/PR-0028-HIGH-PERFORMANCE-ROADMAP.md`.

For the PR-0029 storage path, establish a reproducible measurement contract
before changing queueing, buffering, compression, NUMA placement, or page
size. Report cold/warm cache, worker count, queue capacity, p50/p95 latency,
throughput, peak RSS, and the separate file/CRC/codec/GMP costs.

The full staged roadmap and acceptance gates are in
`docs/PR-0028-HIGH-PERFORMANCE-ROADMAP.md`.

Checkpoint should run on dedicated thread.

Worker threads never perform disk IO.

## PR-0019 Binary Splitting Diagnostic

A local four-worker `-O2` diagnostic used an explicit cutoff of 16 terms and
compared exact sequential and staged-parallel P/Q/T results:

| Terms | Sequential | Parallel |
| ---: | ---: | ---: |
| 64 | 79 us | 394 us |
| 256 | 289 us | 156 us |
| 1024 | 1708 us | 814 us |
| 4096 | 10650 us | 4761 us |

This single-host diagnostic demonstrates both the small-range scheduling cost
and the larger-range benefit; it is not a portable production cutoff. The API
therefore requires an explicit cutoff until repeatable target-CPU benchmarks
cover representative digit counts, worker counts, queue capacities, and warm
cache effects. The execution policy also exposes tasks per worker so later CPU,
NUMA, and queue benchmarks can tune task granularity without changing the
Binary Splitting API.

## PR-0020 End-to-End Benchmark

The standalone `chudnovsky-benchmark` target is excluded from CTest and enabled
with `-DENABLE_BENCHMARKS=ON`. A GCC Release build on an Intel Core Ultra 9
185H used one warm-up, five measured runs, median stage times, cutoff 128, four
tasks per worker, and no CPU affinity.

| Digits | Mode | Workers | Split | Finalize | Format | Total |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 | Sequential | 1 | 70 us | 15 us | 7 us | 95 us |
| 1,000 | Parallel | 4 | 65 us | 13 us | 6 us | 85 us |
| 10,000 | Sequential | 1 | 615 us | 222 us | 78 us | 916 us |
| 10,000 | Parallel | 8 | 312 us | 235 us | 78 us | 627 us |
| 100,000 | Sequential | 1 | 13,299 us | 6,087 us | 2,392 us | 21,762 us |
| 100,000 | Parallel | 4 | 6,136 us | 5,861 us | 2,494 us | 15,911 us |
| 1,000,000 | Sequential | 1 | 247,421 us | 96,755 us | 61,046 us | 416,349 us |
| 1,000,000 | Parallel | 8 | 117,803 us | 94,031 us | 61,056 us | 274,628 us |

Every benchmark mode compares its complete decimal output with the sequential
reference before accepting a sample. Small timings remain sensitive to CPU
frequency and hybrid-core placement. At one million digits, staged splitting
provides useful end-to-end speedup, while integer finalization and decimal
formatting account for roughly 155 ms and now form the next visible serial
bottleneck.

Run the benchmark with:

```bash
cmake -S . -B build-benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_BENCHMARKS=ON
cmake --build build-benchmark --target chudnovsky-benchmark
./build-benchmark/chudnovsky-benchmark
```
