# Windows build measurement

This report records a Release build measurement without prior build artifacts at commit
`88d155dbcb127588a6d1e8d8630c490944d4f29f`.

## Environment

- Windows 11
- GCC 16.2.0 from MSYS2 UCRT64
- CMake 4.4.2
- Ninja 1.13.2
- Two build jobs
- Compiler memory sampled every 200 milliseconds

## Results

| Measurement | Result |
| --- | ---: |
| Build time | 135.545 seconds |
| Peak combined compiler working set | 1,628,827,648 bytes |
| Runtime static library | 18,182 bytes |
| Hello server executable | 2,389,166 bytes |
| OpenAPI exporter executable | 338,869 bytes |

The timer started after CMake configuration. The build target included the
runtime library, hello server, and OpenAPI exporter. Dependencies were already
present. The memory value is a sample of the combined working set for active
`cc1plus` processes. A short peak between samples may not be present in the
result.

The measured target set was narrower than a complete tests and examples build,
but its 135.545 second time still exceeded the proposed 60 second budget. That
budget is not met on this machine.

The script is `benchmarks/measure-build.ps1`. Its output remains below `build` so
that repeated measurements do not change source files.
