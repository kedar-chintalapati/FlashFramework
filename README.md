# Flash

Flash is an experimental C++26 REST API framework. Static reflection provides route,
binding, validation, JSON, and OpenAPI metadata before the server starts.

The reference environment is Windows 11 with MSYS2 UCRT64 GCC 16.2.0,
Boost 1.91.0, CMake 4.4.2, Ninja, and Conan 2.31.2. Reflection code requires
`-freflection`.

Declare endpoints in a namespace and pass its reflection to the server.

```cpp
#include <flash/flash.hpp>

#include <string>

namespace api {
struct Greeting { std::string message; };

[[=flash::get("/hello")]]
Greeting hello() { return {"Hello"}; }

[[=flash::post("/echo")]]
Greeting echo(Greeting body) { return body; }
}

int main() {
    return flash::serve<^^api>({.port = 8080, .workers = 2});
}
```

GET /hello returns JSON. POST /echo binds a JSON body into Greeting.
Development servers expose /openapi.json and /docs using the same type metadata.

## Build and test

```powershell
conan install . --output-folder=build/conan/debug --build=missing -s build_type=Debug
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
.\build\debug\examples\flash_hello.exe
```

The presets expect GCC at C:/msys64/ucrt64/bin/g++.exe and a Conan profile for
Windows, x86_64, GCC 16, libstdc++11, and C++26. The setup guide describes this
profile. In another terminal, request http://127.0.0.1:8080/hello or open /docs.
For Release, install Conan dependencies into build/conan/release with
build_type=Release and use the release presets.

For Linux and installed consumers, see [build instructions](docs/build.md) and
[installation](docs/install.md).

## Editor support

G++ is required for files that use reflection. Current clangd versions do not
parse `^^`, splicers, or annotation expressions. Use the G++ build results for
those files. Keep `-freflection` enabled.

## Design

The compiler discovers annotated functions and derives route, binding, validation,
JSON, and OpenAPI code. The runtime processes requests with the Beast parser,
request views, middleware, route selection, typed binding, the endpoint, and
response serialization. The runtime library contains shared error and version
functions. Server templates and generated adapters remain in public headers.

Endpoint discovery, route checks, binding plans, schemas, and direct invocation
adapters are created during compilation. HTTP parsing, value conversion, JSON
input and output, middleware, and socket work occur for each request.

## Features and limits

Flash supports static, parameter, and catch all paths, scalar and JSON binding,
application state, synchronous and Asio coroutine handlers, domain errors, and
middleware. Tests cover keepalive, concurrency, cancellation, timeouts, and
graceful shutdown. Request and JSON parser limits have finite defaults.

HTTP 1.1 is the only transport. TLS termination, authentication, rate limiting,
and deployment policy belong to the application or its reverse proxy. Streaming,
multipart bodies, WebSockets, and HTTP 2 are deferred. There is no production
support or API compatibility guarantee. See [limits](docs/limits.md).

## Measurements
See [benchmarks](benchmarks/README.md).

## More information

- [Tutorial](docs/tutorial.md).
- [API guide](docs/api.md).
- [Build instructions](docs/build.md).
- [Examples](examples).
- [Security test matrix](docs/security-matrix.md).
