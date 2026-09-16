# Security test matrix

This matrix records the negative tests and limit tests visible in the repository.
It describes tests that exist; it does not claim that unlisted cases are safe.

| Area | Current test or source | What is covered | Missing coverage or deployment responsibility |
| --- | --- | --- | --- |
| Malformed HTTP | `tests/integration/raw_server_test.cpp`; `include/flash/server.hpp` | Malformed request input receives problem JSON. | Exercise request smuggling, conflicting framing, invalid targets, and HTTP protocol edge cases before deployment. |
| Header field count | `tests/integration/raw_server_test.cpp`; `include/flash/config.hpp` | Configured header field limit is checked. | Choose a limit suitable for the deployment and test proxy behavior. |
| Body size | `tests/integration/raw_server_test.cpp`; `include/flash/server.hpp` | Oversized request bodies are rejected. | Set and monitor limits for every deployment path, including proxies. |
| Timeouts | `tests/integration/timeout_test.cpp` | Header/body timeout behavior is exercised. | Set write and idle timeouts for the network and workload. |
| Keepalive | `tests/integration/raw_server_test.cpp`; `tests/integration/typed_server_test.cpp` | Response keepalive and connection close behavior are exercised. | Test client and proxy timeout interaction under load. |
| Cancellation | `tests/integration/shutdown_test.cpp`; `include/flash/server.hpp` | Graceful and forced shutdown cancel active sessions. | Handlers must observe the stop token; verify cleanup of external resources. |
| Shutdown | `tests/integration/shutdown_test.cpp` | Idle, slow, and forced shutdown paths are exercised. | Define termination grace periods and process supervision. |
| Malformed JSON | `tests/unit/json_read_test.cpp`; `tests/unit/adversarial_input_test.cpp` | Invalid numbers, Unicode, trailing input, and malformed structures are tested. | Add application specific payload schemas and fuzzing for new types. |
| JSON limits | `tests/unit/adversarial_input_test.cpp`; `include/flash/json/read.hpp` | Input bytes, depth, strings, arrays, and object members have limit tests. | Align JSON limits with configured HTTP body limits and review limits for workload. |
| Percent decoding | `tests/unit/scalar_binding_test.cpp`; `tests/unit/adversarial_input_test.cpp` | Valid and invalid percent escapes are tested. | Test encoded delimiters and application specific canonicalization rules. |
| Duplicate inputs | `tests/unit/scalar_binding_test.cpp`; `include/flash/binding/request.hpp` | Duplicate query parameters, headers, and cookies are rejected. | Confirm upstream proxies do not merge or rewrite duplicates differently. |
| Binding failures | `tests/unit/typed_dispatch_test.cpp`; `tests/integration/typed_server_test.cpp` | Missing, invalid, constrained, and unsupported media type inputs produce problem responses. | Review error status and disclosure requirements for each API. |
| Exception redaction | `tests/unit/middleware_test.cpp`; `tests/integration/typed_server_test.cpp` | Handler exception details are not returned to clients. | Ensure logs and telemetry also protect secrets and personal data. |
| Middleware identifiers and logging | `tests/unit/middleware_test.cpp`; `include/flash/middleware.hpp` | Request IDs, access logging callbacks, and middleware exception recovery are exercised. | Protect logs, synchronize shared callback state, and define retention and access controls. |
| Compile-time declaration failures | `tests/compile_fail/*.cpp` | Invalid routes, bindings, duplicate declarations, error mappings, and OpenAPI collisions are compile-fail cases. | Keep compile-fail tests current when annotations or generated routing change. |
| Sanitizer CI | `.github/workflows/ci.yml` | Linux Debug CI enables ASAN and UBSAN with leak detection and runs tests. | Run sanitizer builds for new platform/compiler combinations and supplement with fuzzing. |

## Explicit product and deployment gaps

- There is no TLS implementation in this framework. Terminate TLS at a trusted proxy or
  provide TLS before exposing the server to an untrusted network.
- There is no authentication or authorization layer. Applications must enforce identity,
  access control, and tenant isolation.
- There is no CORS policy implementation. Applications or a front proxy must set and
  validate the required policy.
- There is no global connection cap, per client rate limit, or request admission control.
  Operators must provide these controls upstream and monitor resource exhaustion.
- This matrix does not represent an independent security audit. Arrange a security review
  before production use, especially for custom handlers, middleware, and proxy settings.
- Deployment owners remain responsible for threat modeling, patching, monitoring, incident response,
  and secret handling.
