# Initial Windows measurements

These measurements describe commit ab3775acd989690e68ef037f3ec9a6d8b0ae27ba.
They are an initial engineering baseline for the experimental release.

The machine ran Windows 11 Home version 10.0.26200 on an Intel Core i7 13620H
with 16 logical processors. The compiler was MSYS2 UCRT64 GCC 16.2.0 Rev3.
The native preset used O3 and local CPU tuning. Boost was 1.91.0.

## Method

Run the following command from the repository root.

```powershell
.\benchmarks\run.ps1 -RequestsPerConnection 1000
```

The run covered eight workloads, two servers, one and two workers, four
connection counts, and three trials. All 384 records reported zero errors.
Each connection performed 20 warmup requests. Larger bodies and timer workloads
used fewer measured requests as specified in the runner. Results below use the
median of the three trials.

The load client and server shared this laptop. No CPU affinity was configured.
Latency is client round trip latency. Server CPU time includes warmup and has
limited resolution for short cases. These measurements do not establish isolated
server overhead or capacity on another machine.

The manual baseline uses Beast and the same Flash JSON codec. It omits Flash
session stop tracking, header view construction, and some request validation.
Its session executor also differs from the Flash session strand. This comparison
therefore includes runtime service differences as well as typed dispatch costs.
An equivalent lifecycle baseline is needed to isolate typed dispatch overhead.

## Selected comparisons

All values below use one server worker. Throughput is requests per second.

| Workload | Connections | Flash | Manual | Flash percent of manual |
| --- | --- | --- | --- | --- |
| Health | 1 | 14873 | 17853 | 83.3 |
| Health | 8 | 31560 | 36214 | 87.2 |
| Health | 32 | 30638 | 36218 | 84.6 |
| Health | 128 | 28784 | 33710 | 85.4 |
| Integer addition | 1 | 18114 | 17291 | 104.8 |
| Integer addition | 8 | 31843 | 37309 | 85.3 |
| JSON echo about 64 KiB | 1 | 1207 | 1402 | 86.1 |
| JSON echo about 64 KiB | 8 | 1595 | 2164 | 73.7 |

The proposed 90 percent throughput target was missed in several cases.
Throughput stops increasing at about eight connections for the one worker trivial
routes, so higher connection counts include queueing. These results do not
establish the proposed isolated median or p99 server overhead limits.

The one millisecond timer workload had median round trip latency near 16
milliseconds for both servers. Timer resolution and scheduling dominate that
workload on this machine.

## Allocation boundaries

The allocation executable measured 10000 operations after warmup.

| Boundary | Allocations per operation |
| --- | --- |
| Route match and two integer path bindings | 0 |
| Query scan over eight keys | 0 |
| Percent decoding with owned output | 1 |
| JSON read into a fixed size object | 0 |
| JSON write into a reserved reusable string | 0 |
| JSON read with string and vector ownership | 4 |
| Problem object and response serialization | 6 |
| Response with three owned headers | 5 |

These counts exclude transport, sockets, coroutine frames, and operating system
allocations. Zero counts apply only to the named boundaries.

## Follow up

Endpoint selection currently adds coroutine frames while passing earlier
endpoints. Remove these frames and measure again. Align baseline lifecycle and
validation costs before attributing the full difference to generated binding.
Measure generated APIs with larger route counts separately from the existing
runtime route scan microbenchmark.
