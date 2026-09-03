# ADR 0001: GCC 16.2 is the reference compiler

- Status: accepted
- Date: 2026-09-03

## Context

Flash's primary API depends on C++26 static reflection, annotations, function-parameter reflection, and splicers. The installed MSYS2 UCRT64 GCC 16.2 toolchain supports the required experiment behind `-std=c++26 -freflection`; current upstream Clang and MSVC releases do not provide an equivalent implementation.

The shipped GCC 16.2 `<meta>` interface requires an explicit `std::meta::access_context` argument for member queries. This differs from early examples that used one-argument forms.

## Decision

The experimental v0.1 line uses GCC 16.2 as its sole supported reflection compiler. `flash::meta` propagates `-freflection` to consumers, and all member discovery passes an explicit access context. Reflection-heavy files remain isolated from the compiled runtime where practical.

## Consequences

- G++ build and test diagnostics are authoritative for reflection code.
- clangd may report parse errors in reflection-heavy headers even when G++ accepts them.
- Public releases must pin the exact tested compiler revision.
- Stable `FLASH-E###` diagnostic fragments are checked by expected-compile-failure tests.
- A compiler upgrade requires running the reflection and compile-failure suites before changing the supported matrix.

