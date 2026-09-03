# Flash

Flash is an experimental C++26 REST framework exploring how much of a modern web framework can disappear before the program runs.

The project is under active construction. Its reference environment is native Windows with MSYS2 UCRT64 GCC 16.2, CMake 4.4, Ninja, Conan 2, and the compiler's `-freflection` implementation of C++26 static reflection.

## Current status

The repository is being built milestone by milestone from the accompanying design specification. The initial scaffold separates the reflection-facing interface from a compiled runtime library and pins the intended toolchain and dependency workflow.

See [PROGRESS.md](PROGRESS.md) for the current checkpoint and [flash-framework-spec-and-design.md](flash-framework-spec-and-design.md) for the proposed v0.1 design.

## Configure and test

```powershell
conan install . --output-folder=build/conan/debug --build=missing -s build_type=Debug
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Flash is not production-ready. The API, supported compiler revision, and implementation are expected to change during the experimental v0.1 cycle.

