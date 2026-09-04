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

## Workload suite

`flash_benchmark_typed` and `flash_benchmark_manual` provide matching routes for
text, integer path parsing, JSON output, JSON echo, validation failure, and an
Asio timer wait. Both JSON implementations use the Flash JSON codec. The manual
server uses handwritten route and argument handling.

`flash_benchmark_load` opens one keepalive connection per client thread. It runs
one unmeasured request on each connection, then records throughput, errors, and
p50, p90, p99, and p99.9 latency.

Run the matrix from PowerShell.

```powershell
.\benchmarks\run.ps1
```

The script builds the native preset with two jobs. It tests 1, 8, 32, and 128
connections. JSON bodies cover about 100 bytes, 1 KiB, and 64 KiB. Results and
machine metadata are written below `build\benchmark-results` and remain local
until they are reviewed for publication.
