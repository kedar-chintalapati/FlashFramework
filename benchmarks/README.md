# Benchmark method

Flash and the baseline use Boost.Asio and Boost.Beast with the same request and
response behavior.

- `flash_benchmark_raw` uses the Flash raw handler.
- `flash_benchmark_beast` uses a direct Beast handler for the same health route.

Build with the `native` preset. Use the same port, worker count, connection count,
keepalive setting, payload, and load generator settings for both programs. Native
binaries are specific to the build machine.

Record the Git commit, CPU, Windows version, compiler, command, warmup period,
test duration, throughput, connection count, latency percentiles, CPU use, and
error count.

`flash_benchmark_request_view` measures request view construction and header
lookup. It is not a server benchmark.

`counting_resource` measures allocations made through its PMR boundary. It does
not measure all process allocations.
