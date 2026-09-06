# Operational limits

Default HTTP limits are 16 KiB of headers, 100 header fields, and 1 MiB of body.
Header reads have five seconds. Body reads and response writes have fifteen
seconds. Keepalive idle reads have sixty seconds. Graceful shutdown allows ten
seconds before forced cancellation. Configure these values in server_config.

JSON defaults allow 1 MiB input, depth 64, 1 MiB strings, and 100000 array
elements or object members. Direct JSON callers can supply read_limits.
Typed request binding currently uses the default JSON limits. Route annotations
hold at most 255 bytes, 32 segments, and 16 captures.

These are per request limits. There is currently no global connection cap or
per client rate limit. Applications exposed to untrusted traffic need an
upstream connection limit and request rate policy. TLS, authentication, CORS,
and authorization are application responsibilities.

Handlers share worker threads. Blocking a worker delays unrelated requests.
Use a separate executor for blocking work. A synchronous handler that ignores
stop cannot be forcibly interrupted by the framework. Forced shutdown cancels
pending asynchronous operations and eventually stops the I/O context.

Responses and bodies are buffered. Large output values can allocate according
to application data. Borrowed request data becomes invalid when the request
finishes. Do not store request views in background tasks.

The project is experimental and has not had an independent security audit.
The benchmark report documents missed targets and comparison limitations.
