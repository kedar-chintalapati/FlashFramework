# Tutorial

Start with examples/hello/main.cpp and build the flash_hello target.
The server listens on loopback port 8080. Request /hello for text and /add/20/22
for a scalar JSON result. Application arithmetic must handle its own overflow
and domain rules.

## JSON requests

Define a public aggregate for the body. Parameter and member annotations use
the GCC reflection annotation syntax.

```cpp
struct CreateItem {
    [[=flash::min_length(1), =flash::max_length(80)]]
    std::string name;
    [[=flash::minimum(1)]]
    int quantity{};
};

namespace api {
[[=flash::post("/items")]]
CreateItem create(CreateItem body) { return body; }
}
```

Send a Content-Type of application/json. Flash decodes and validates the body
before calling the endpoint. Invalid values produce a 422 problem response.
An unsupported media type produces 415. Required fields must be present.
Optional fields may be omitted or null.

## Application state and async work

Create application services before starting the server. Pass them by reference
to serve. An endpoint requests one with flash::state<Service>. The exact service
type selects the registered instance. Protect shared mutable state because
different requests may execute concurrently.

An asynchronous endpoint returns flash::task<T> and uses co_await with Asio
operations. The request context carries a stop token. Stop is cooperative while
handlers are running. Keep borrowed input inside the request lifetime.
examples/blocking shows work moved to a separate Asio thread pool.

## Documentation

The development handler serves /openapi.json and /docs. Build the
flash_export_openapi target to write an example document without starting a
server. Select flash::openapi::documentation_mode::disabled as the second
serve template argument to remove these routes from an application.

Use examples/raw when you need request views and explicit response construction.
The raw handler still uses the same HTTP parser, timeouts, and server lifecycle.
