# Benchmark methodology

Flash performance claims are made only against equivalent behavior on the same transport substrate. The initial pair is:

- `flash_benchmark_raw`: Flash's raw handler path over its Beast/Asio session runtime.
- `flash_benchmark_beast`: handwritten Beast/Asio handling the same `GET /health` request and `text/plain` response.

Build both with the `native` preset, pin client and server conditions consistently, and use the same port, worker count, connection count, keep-alive policy, payload, and load generator settings. The `native` binaries are local-machine artifacts and must not be redistributed as portable releases.

Every published result must record the Git commit, CPU, Windows version, compiler, exact command, warm-up, duration, achieved throughput, connection count, p50/p90/p99/p99.9 latency, CPU utilization, and errors. A single maximum requests-per-second number is insufficient.

`flash_benchmark_request_view` is a narrow microbenchmark used only to catch gross regressions in request-view construction and case-insensitive header lookup. It is not an end-to-end server result.

The benchmark-only `counting_resource` reports allocations made through an explicitly instrumented PMR boundary. Its numbers must not be described as whole-process allocation counts.

