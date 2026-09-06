# Build Flash

Use GCC 16.2.0 with C++26 and reflection enabled. Boost 1.91.0 is supplied by
Conan. CMake 4.4.2 and Ninja build the project. Other compilers are unsupported.

## Windows

Install MSYS2 UCRT64 GCC and Ninja. Keep its ucrt64/bin directory on PATH.
Use native Windows Python with Conan 2.31.2. Do not mix MINGW64 and UCRT64
libraries. The checked in presets select C:/msys64/ucrt64/bin/g++.exe.

The Conan host profile needs these settings.

```ini
[settings]
os=Windows
arch=x86_64
compiler=gcc
compiler.version=16
compiler.libcxx=libstdc++11
compiler.cppstd=26
build_type=Release
```

Install separate Debug and Release dependency graphs as shown in the README.
All checked in build presets use two jobs. clangd cannot validate reflection
syntax in the supported setup. Use GCC diagnostics for those files.

## Linux

CI uses the official gcc:16.2.0 image. With that compiler on PATH, use these
commands after installing CMake, Ninja, Python, and Conan.

```sh
conan profile detect
conan install . --output-folder=build/conan/linux --build=missing \
  -s compiler.cppstd=26 -s build_type=Debug
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/conan/linux/conan_toolchain.cmake" \
  -DFLASH_SANITIZERS=ON
cmake --build build/linux --parallel 2
ctest --test-dir build/linux --output-on-failure
```

FLASH_SANITIZERS enables address and undefined behavior checks on GCC Linux.
Use a separate build directory for an ordinary Release build. The native preset
is for measurements on the current Windows machine and is not a portable build.

Install openapi-spec-validator version 0.9.0 into the Python environment selected
by CMake to enable the external OpenAPI test. That test reports skipped when the
validator is missing. CI installs it explicitly.
