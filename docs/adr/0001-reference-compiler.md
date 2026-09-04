# ADR 0001. Reference compiler

Status is accepted.

Date is 2026-09-03.

## Context

Flash uses C++26 reflection, annotations, parameter reflection, and splicers.
MSYS2 UCRT64 GCC 16.2 provides these features with `-std=c++26 -freflection`.
Current Clang and MSVC versions do not provide the required implementation.

GCC 16.2 requires an explicit `std::meta::access_context` for member queries.

## Decision

Flash v0.1 supports GCC 16.2 for reflection code. The `flash::meta` target supplies
`-freflection`. Member discovery always supplies an access context. Networking
code remains in the compiled runtime library when possible.

## Results

- G++ results define whether reflection code is valid.
- clangd can report incorrect errors in reflection headers.
- Releases must state the tested GCC version.
- Compile failure tests check stable `FLASH-E` codes.
- Compiler upgrades require the reflection and compile failure tests.
