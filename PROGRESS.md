# Implementation progress

This file is an interruption safe checkpoint for the Flash v0.1 implementation.

## Recovery log

- 2026-09-03. Resumed after a usage limit interruption at clean commit `d97e841`.
  No partial M4 files were present. The next unit is reflected JSON schema
  metadata, followed by the native cursor/codec and typed body integration.

## Current checkpoint

- Repository initialized on `main` and connected to the empty private GitHub origin.
- Native toolchain verified. UCRT64 G++ 16.2, CMake 4.4.2, and Ninja 1.13.2.
- GCC reflection probe verified after accounting for the shipped two argument `std::meta::members_of` and `nonstatic_data_members_of` APIs, which require an explicit `std::meta::access_context`.
- CMake, Conan, target split, warnings, presets, version smoke test, and initial documentation scaffolded.
- The proposed Boost 1.92 Conan recipe is not published by the configured Conan Center remote. The project therefore pins the newest available recipe, Boost 1.91.0.
- Route/source/validation annotation values, reflected namespace discovery, function parameter and aggregate field inspection, and direct reflected invocation pass in Debug and Release.
- Compile failure tests verify stable Flash diagnostic identifiers for missing and duplicate route annotations.
- Transport neutral request views, validated response headers, finite server defaults, and RFC 9457-style problem serialization are implemented with unit coverage.
- The first Beast/Asio coroutine server supports bounded header/body parsing, finite deadlines, sequential keepalive, raw async handlers, validated responses, and cooperative stop.
- Loopback integration coverage includes keepalive reuse, connection close, 404 handling, oversized request rejection, and malformed header rejection.
- A bounded concurrency test drives eight simultaneous keepalive clients through two I/O workers and verifies every response and handler invocation.
- Consteval route parsing now normalizes literal, parameter, and catch all segments. The generated API matcher implements deterministic specificity, captures, 404/405, implicit HEAD, automatic OPTIONS, and `Allow` calculation.
- Strict scalar codecs cover booleans, integral/floating range checks, strings, reflected enum names, percent decoding, and bounded duplicate aware query/header/cookie lookup.
- Generated typed adapters infer path/query/body/context sources, honor explicit header/cookie/default metadata, bind into reflected parameter types, invoke functions directly through splicers, and map scalar/text/void results plus routing errors into HTTP responses.
- The reflected dispatcher is connected to the Beast runtime. The hello example and loopback integration test exercise real typed GET/DELETE routes and 422 errors over HTTP/1.1.
- Allocation instrumentation verifies zero calls to global `new` across 10,000 successful generated route matches plus two integer path bindings. Transport, coroutine, response, and user code allocations are explicitly outside that test boundary.
- Reflected JSON schema metadata now exposes aggregate field names, aliases, types,
  constraints, and direct member splicers. Input/output schema validation diagnoses
  invalid aggregates, inaccessible fields, ambiguous aliases, duplicate wire names,
  and non default constructible request objects. Focused Debug and compile failure
  coverage pass.
- The native JSON codec reads and writes reflected aggregates, vectors, fixed arrays,
  optionals, strings, numbers, booleans, and enums. It applies finite limits, UTF-8
  and Unicode escape checks, required and duplicate field rules, defaults, aliases,
  and numeric, length, and item constraints.
- Inferred and explicit JSON request bodies now bind through generated adapters.
  Media types are checked, multiple body parameters fail at compile time, and a
  typed POST returning another reflected aggregate passes direct and loopback tests.
- All 20 tests pass in both Debug and Release at this checkpoint.
- `std::expected<T, E>` and `std::expected<void, E>` response adaptation uses a
  compile time enum mapping. Missing or duplicate enum cases produce stable
  diagnostics, and mapped failures use the common problem response schema.
- OpenAPI 3.1.1 generation now uses route, binding, response, error, and JSON
  schema metadata shared with runtime behavior. Output ordering is deterministic,
  recursive object schemas use component references, and component and operation
  name collisions produce compile time diagnostics.
- Development handlers serve `/openapi.json` and an embedded `/docs` page. Both
  routes can be removed through a compile time mode. A build target exports the
  document, and tests compare exact bytes with a golden file.
- The exported document passes the pinned `openapi-spec-validator` 0.9.0 in an
  isolated local environment. All 29 configured Debug and Release tests pass.
- Application state is resolved by exact type and stored outside request data.
  Missing and duplicate registrations have stable compile diagnostics. Direct and
  loopback tests cover state access across an Asio suspension. All 31 Debug tests
  pass at this checkpoint.
- A typed middleware chain now wraps dispatch. Built in request ID, access log,
  and exception recovery middleware pass unit and loopback tests. Request IDs
  reach endpoint context, response headers, problem bodies, and log records.
  Invalid middleware return types produce `FLASH-E700`. All 33 Debug tests pass.
- Server stop now closes the listener, requests cooperative handler stop, cancels
  idle reads, drains active responses, and forces cancellation after the configured
  deadline. Header, body, and keepalive deadlines have loopback coverage. Session
  control is owned by each coroutine and serialized on a strand. All 35 Debug
  tests pass, including repeated shutdown and concurrency runs.
- The blocking work example stores an Asio thread pool in application state and
  awaits a task spawned on that pool. A unit test confirms that work runs on a
  different thread and returns through the request coroutine. All 36 Debug and
  Release tests pass. M6 is complete.

## Milestones

- Complete. M0 compiler and reflection spike.
- Complete. M1 HTTP runtime and raw endpoints.
- Complete. M2 compile time routes.
- Complete. M3 typed scalar binding.
- Complete. M4 reflected schema and JSON.
- Complete. M5 responses, errors, and OpenAPI.
- Complete. M6 async, state, and middleware.
- Active. M7 performance hardening and experimental release.

## Immediate next work

1. Complete the benchmark and allocation matrix.
2. Measure compile time, compiler memory, and binary size.
3. Add CI, package installation, release documentation, and security tests.

## Safety and recovery notes

- Build presets intentionally cap Ninja at two jobs while the new reflection implementation is being characterized.
- Git and compiler processes are checked between longer stages. No Git process was
  left running at this recovery point.
- Generated build output and local benchmark measurements remain untracked.
- Git history and this file record completed work. Experimental work should pass
  its focused tests before it is pushed.
