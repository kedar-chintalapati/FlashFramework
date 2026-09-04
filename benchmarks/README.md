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

`flash_benchmark_core` emits JSON lines for method masks, route scans at 1, 10,
100, and 1,000 routes, parameter and catch all paths, scalar parsing, query scans,
JSON reads and writes, validation failure, problem responses, and response
headers. Each record includes iterations, bytes, time, cycles on x86, and a
checksum used to retain the measured work.

`flash_benchmark_allocations` intercepts global allocation only inside each
named measurement loop. It reports route and binding work, query scans, owned
percent decoding, fixed and owned JSON values, problem responses, and response
headers. The boundary field identifies which framework and user object work is
included. Transport, coroutine, socket, operating system, and load client
allocations are outside this executable.

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
connections with one and two server workers. JSON bodies cover about 100 bytes,
1 KiB, and 64 KiB. Results, core measurements, allocation counts, and machine
metadata are written below `build\benchmark-results` and remain local until they
are reviewed for publication.
