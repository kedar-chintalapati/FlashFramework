# Install Flash

Build with the pinned GCC and Conan dependency setup described in the README.
Install the Release build into a prefix of your choice.

```powershell
cmake --preset release
cmake --build --preset release --parallel 2
cmake --install build/release --prefix C:/path/to/flash
```

Consumers use the installed CMake package.

```cmake
find_package(flash 0.1.0 EXACT CONFIG REQUIRED)
target_link_libraries(my_server PRIVATE flash::flash)
```

Set flash_DIR to the installed lib/cmake/flash directory. Supply the same Boost
1.91.0 dependency through the consumer Conan toolchain. Flash exports the runtime,
meta, and flash targets under the flash namespace. The meta target propagates
the reflection compiler flag. The flash target links both layers.

The experimental package uses exact version matching. Compile consumers with
the same compiler and compatible runtime settings as the installed library.
Native CPU tuning is confined to local benchmark targets.

The tests/package directory contains an independent consumer. It verifies
runtime linkage and reflected JSON through installed headers. A local Windows
verification can use the existing Release Conan toolchain.

```powershell
cmake --install build/release --prefix build/package-check/prefix
cmake -S tests/package -B build/package-check/consumer -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/conan/release/conan_toolchain.cmake" `
  -Dflash_DIR="$PWD/build/package-check/prefix/lib/cmake/flash"
cmake --build build/package-check/consumer --parallel 2
ctest --test-dir build/package-check/consumer --output-on-failure
```
