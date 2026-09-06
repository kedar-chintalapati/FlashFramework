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
- The native benchmark harness now compares typed Flash routes with matching
  handwritten Beast routes. It covers text, integer binding, JSON output, JSON
  echo at three body sizes, validation failure, and an Asio timer wait. The load
  client records throughput, latency percentiles, and errors. A small smoke run
  passed all 16 server and workload combinations with zero errors.
- The benchmark matrix now covers one and two server workers. A second smoke run
  passed all 32 combinations. Core microbenchmarks emit 22 JSON records for
  routing, binding, query, JSON, validation, error, and header work. A separate
  global allocation executable reports eight named boundaries. Route plus two
  integer bindings, fixed object JSON read, and reused buffer JSON write each
  recorded zero allocations over 10,000 measured operations.
- End to end measurements now default to three trials after 20 warmup requests
  per connection. Records include trial identity and server CPU time. A focused
  smoke run verified all output fields and all 32 server cases with zero errors.

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

The complete initial Windows matrix at commit ab3775a passed 384 records with
zero errors. Raw files remain in build/benchmark-results/release-ab3775a-windows.
The reviewed initial report is docs/benchmarks/windows-initial.md. Several
throughput gates were missed. The manual baseline has different lifecycle and
strand behavior, so it cannot isolate typed dispatch overhead.

Endpoint selection now returns the selected task without adding a coroutine
for every earlier endpoint. Debug dispatch, middleware, and typed server tests
passed. The native typed target rebuilt without warnings. Nine focused network
trials passed with zero errors but showed timing variation, so no speedup claim
is established by that run.

CMake installation and an independent consumer are implemented. The Release
install into build/package-check/prefix passed the consumer build and CTest.
The consumer verifies compiled runtime linkage and reflected JSON from installed
headers. Instructions are in docs/install.md.

1. Complete the benchmark and allocation matrix.
2. Measure compile time, compiler memory, and binary size.
3. Add CI, package installation, release documentation, and security tests.

The new adversarial input test passes in Debug. It checks malformed JSON,
Unicode escapes, duplicate fields and query keys, size and depth limits, and
10000 deterministic mutations with round trips for accepted values. The core
benchmark floating point checksum now uses bit_cast to avoid converting a
negative floating point value to an unsigned integer. The native benchmark
rebuilt and ran successfully after this correction.

CI now defines Windows Debug and Release jobs and Linux GCC 16.2.0 sanitizer
coverage. Upstream action revisions are pinned. GitHub Actions is enabled.
The CI definition is committed as 88d155d. GitHub Actions run 33948817738 passed
the Windows Debug, Windows Release, and Linux sanitizer jobs.

The clean Release build measurement at commit 88d155d completed in 135.545
seconds with two jobs. The sampled combined compiler working set peaked at
1,628,827,648 bytes. Output sizes were 18,182 bytes for the runtime library,
2,389,166 bytes for the hello server, and 338,869 bytes for the OpenAPI exporter.
The reviewed result is in docs/benchmarks/windows-build.md.

The release documentation draft now includes the quickstart, architecture flow,
API guide, build instructions, operational limits, roadmap, contribution guide,
conduct policy, security process, and license. Relative links resolve and the
new prose is plain ASCII.

PUT and PATCH now have typed aggregate body tests in direct dispatch and over a
loopback HTTP connection. Explicit OPTIONS dispatch is covered alongside the
existing automatic OPTIONS, implicit HEAD, explicit HEAD, and DELETE cases. The
focused routing and server tests pass in Debug and Release.

Complete example sources now cover hello and typed paths, CRUD with state and
domain errors, an Asio timer, explicit blocking work, raw responses, versioning,
and OpenAPI export. The new CRUD and timer targets compile in Debug and Release.
Release loopback smoke requests verified create, read, patch, delete, and the
asynchronous timer response.

The compile failure suite now has a raw handler contract case, so each public
diagnostic number group from endpoint discovery through middleware is represented.
The new `FLASH-E101` case passes in Debug.

Opt in synthetic targets now generate reflected APIs with 1, 10, 100, and 1,000
routes. A fresh build measurement script uses two jobs and a 2.5 GiB sampled
compiler memory safety limit. The original recursive conflict check exceeded
GCC constant evaluation depth at 100 routes. It now uses compiled route arrays,
loops, and a hash filter before exact comparison. Focused Debug and Release tests
pass. Generated 1, 10, and 100 route executables compile and pass their runtime
checks. The 1,000 route attempt reached the memory guard and stopped cleanly.

Final measurements at commit 48a414f record 1, 10, and 100 route build times,
compiler memory, executable size, and text size. The 100 route target completed
in 30.177 seconds with 573,796,352 bytes of sampled compiler working set. The
1,000 route target crossed a 2 GiB guard after 65.093 seconds and wrote a failure
report before stopping. The reviewed report is in
docs/benchmarks/windows-routes.md.

OpenAPI now emits descriptions and deprecation flags for parameters and JSON
fields as well as endpoints. The focused schema and document tests pass in Debug.
The golden export is unchanged and its local structural check passes. The
external validator remains available in CI. Pattern and custom callable
validation are explicitly deferred in the API guide and roadmap.

The public version is now 0.1.0 and the changelog records features and known
limits. Header and runtime versions agree in focused Debug and Release tests.
Full builds, package installation, clean clone, final CI, and tag checks remain
before release.

## Safety and recovery notes

- Build presets intentionally cap Ninja at two jobs while the new reflection implementation is being characterized.
- Git and compiler processes are checked between longer stages. No Git process was
  left running at this recovery point.
- Generated build output and local benchmark measurements remain untracked.
- Git history and this file record completed work. Experimental work should pass
  its focused tests before it is pushed.
