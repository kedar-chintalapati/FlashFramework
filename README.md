# Flash

Flash is an experimental C++26 REST framework. Static reflection provides route,
binding, validation, JSON, and OpenAPI metadata before the server starts.

The reference environment is Windows 11 with MSYS2 UCRT64 GCC 16.2, CMake 4.4,
Ninja, and Conan 2. Reflection code requires `-freflection`.

## Status

Flash is under development and is not ready for production. The API and compiler
requirements can change during the v0.1 release cycle.

See [PROGRESS.md](PROGRESS.md) for the current work and
[flash framework spec and design.md](flash-framework-spec-and-design.md) for the
v0.1 requirements.

## Build and test

```powershell
conan install . --output-folder=build/conan/debug --build=missing -s build_type=Debug
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Editor support

G++ is required for files that use reflection. Current clangd versions do not
parse `^^`, splicers, or annotation expressions. Use the G++ build results for
those files. Keep `-freflection` enabled.
