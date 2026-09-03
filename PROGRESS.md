# Implementation progress

This file is an interruption-safe checkpoint for the Flash v0.1 implementation.

## Current checkpoint

- Repository initialized on `main` and connected to the empty private GitHub origin.
- Native toolchain verified: UCRT64 G++ 16.2, CMake 4.4.2, and Ninja 1.13.2.
- GCC reflection probe verified after accounting for the shipped two-argument `std::meta::members_of` and `nonstatic_data_members_of` APIs, which require an explicit `std::meta::access_context`.
- CMake, Conan, target split, warnings, presets, version smoke test, and initial documentation scaffolded.
- The proposed Boost 1.92 Conan recipe is not published by the configured Conan Center remote; the project therefore pins the newest available recipe, Boost 1.91.0.
- Route/source/validation annotation values, reflected namespace discovery, function-parameter and aggregate-field inspection, and direct reflected invocation pass in Debug and Release.
- Expected-compile-failure tests verify stable Flash diagnostic identifiers for missing and duplicate route annotations.
- Transport-neutral request views, validated response headers, finite server defaults, and RFC 9457-style problem serialization are implemented with unit coverage.
- The first Beast/Asio coroutine server supports bounded header/body parsing, finite deadlines, sequential keep-alive, raw async handlers, validated responses, and cooperative stop.
- Loopback integration coverage includes keep-alive reuse, connection close, 404 handling, oversized request rejection, and malformed-header rejection.
- A bounded concurrency test drives eight simultaneous keep-alive clients through two I/O workers and verifies every response and handler invocation.

## Milestones

- [x] M0: compiler and reflection spike
- [x] M1: HTTP runtime and raw endpoints
- [ ] M2: compile-time routes
- [ ] M3: typed scalar binding
- [ ] M4: reflected schema and JSON
- [ ] M5: responses, errors, and OpenAPI
- [ ] M6: async, state, and middleware
- [ ] M7: performance hardening and experimental release

## Immediate next work

1. Implement the consteval route grammar and normalized route shape.
2. Reject route conflicts with stable compile diagnostics.
3. Dispatch static, parameter, and catch-all routes with 404/405/HEAD/OPTIONS semantics.

## Safety and recovery notes

- Build presets intentionally cap Ninja at two jobs while the new reflection implementation is being characterized.
- Generated build output and local benchmark measurements remain untracked.
- The project source of truth is the Git history plus this checkpoint; incomplete experimental work should not be pushed until it forms a coherent commit.
