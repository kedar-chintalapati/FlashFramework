# Windows profile guided measurement

This report records GCC profile generation and use for the core request
processing benchmark at commit `eafcc0f94022c4dee3ab3ff503838c30c781814b`.

## Environment

- Windows 11 Home 10.0.26200.
- Intel Core i7-13620H with 16 logical processors.
- MSYS2 UCRT64 GCC 16.2.0.
- Release mode with native CPU optimization.
- Two build jobs.

## Method

The command was:

```powershell
.\benchmarks\profile-request.ps1 -Iterations 500000 -Trials 3
```

The script built an ordinary baseline and a profile generation build. It ran the
generation build once over every core benchmark case, then reconfigured the same
build path to use the generated data. Reusing the build path is required because
GCC includes the object path in the profile file name. Baseline and profile use
binaries each ran three measured trials.

GCC produced one 39,928 byte profile data file. The saved use build log contains
the `-fprofile-use`, `-fprofile-correction`, and `-Wmissing-profile` options on the
compile command. It contains no missing profile warning. The result is stored
locally under `build/benchmark-results/pgo-20260906-232036-692`.

An earlier gprof attempt produced a nonempty data file but no function records on
this Windows toolchain. It was rejected. The repository command uses GCC profile
generation and use instead.

## Results

Values are the median nanoseconds per operation from three trials. A negative
change means the profile use binary was faster. All baseline and profile use
checksums matched for every case.

| Case | Baseline ns | Profile use ns | Change percent |
| --- | ---: | ---: | ---: |
| method mask | 0.954 | 0.956 | 0.14 |
| literal route, 1 | 29.125 | 7.838 | -73.09 |
| literal route, 10 | 256.826 | 172.238 | -32.94 |
| literal route, 100 | 2478.840 | 1549.960 | -37.47 |
| literal route, 1000 | 16625.000 | 14206.800 | -14.55 |
| parameter route | 31.361 | 28.365 | -9.55 |
| catch all route | 23.896 | 24.546 | 2.72 |
| integer parsing | 7.701 | 5.453 | -29.19 |
| floating point parsing | 18.081 | 16.581 | -8.30 |
| boolean parsing | 3.143 | 1.886 | -39.97 |
| query scan, 1 key | 27.212 | 24.739 | -9.09 |
| query scan, 8 keys | 160.535 | 140.086 | -12.74 |
| query scan, 32 keys | 683.718 | 646.731 | -5.41 |
| JSON read, 100 bytes | 334.914 | 223.614 | -33.23 |
| JSON read, 1 KiB | 1413.330 | 1164.180 | -17.63 |
| JSON read, 64 KiB | 102614.000 | 90079.600 | -12.22 |
| JSON write, 100 bytes | 224.310 | 95.404 | -57.47 |
| JSON write, 1 KiB | 2330.340 | 939.450 | -59.69 |
| JSON write, 64 KiB | 148799.000 | 60710.200 | -59.20 |
| validation failure | 277.035 | 168.453 | -39.19 |
| problem response | 524.260 | 466.460 | -11.03 |
| response headers | 723.564 | 664.668 | -8.14 |

Trial spread was large in several cases, especially the 64 KiB JSON read.
The literal route, JSON write, and validation cases had distinct baseline and
profile use values. The method mask and
catch all route cases were effectively unchanged at this sample size.

## Decision

The GCC profile workflow is available as an optional benchmark tool. It is not
enabled for the public package. The measured training run includes all benchmark
cases in one combined benchmark workload, so it does not justify a framework
source change or a claim about HTTP throughput, sockets, Beast, coroutines, or
production workloads.

Follow up work will profile representative request mixes and confirm any proposed
source change with the end to end benchmark matrix. Route matcher structure and
owned response header work remain candidates, but this report does not select an
implementation change.
