# API guide

Include flash/flash.hpp for the typed API. Link flash::flash with CMake.
Include flash/server.hpp and link flash::runtime for a raw server.

## Routes

Route annotations are get, post, put, patch, delete_, head, and options.
A path starts with a slash. Literal segments outrank parameters, which outrank
a final catch all segment. Examples are /users/me, /users/{id}, and
/assets/{*path}. Trailing slashes are strict. Duplicate method and route shapes
fail compilation. Method mismatches return 405 with Allow. OPTIONS is automatic
unless an endpoint declares it. HEAD can use a GET endpoint and suppresses its
body, so that endpoint may still execute application code.

## Binding

A parameter whose name matches a path placeholder binds from that placeholder.
Remaining scalars normally bind from query keys. Public aggregate parameters on
POST, PUT, and PATCH infer a JSON body. Explicit path, query, header, cookie,
and body annotations select sources. One endpoint may have one body parameter.
Context and state wrapper types select request context and application services.

Scalar types include integers, finite floating point values, bool, string,
string_view, and reflected enums. Optional scalar parameters accept absence.
Use default_value annotations for defaults. Native C++ default expressions are
not extracted. Duplicate scalar query keys, headers, and cookies are rejected.
Path segmentation occurs before percent decoding. A decoded slash remains
inside its captured value.

JSON supports public aggregates, arrays, vectors, optional values, strings,
numbers, bool, and enums. Unknown and duplicate object fields are rejected by
default. Field name aliases use name. Constraints include minimum, maximum,
exclusive numeric bounds, min_length, max_length, min_items, and max_items.
String lengths measure encoded bytes. Borrowed string views cannot decode
escaped strings and must not outlive the input buffer.

Description and deprecated annotations appear in generated OpenAPI for
endpoints, parameters, and JSON fields. Pattern and custom callable validation
are deferred. Do not rely on the reserved pattern annotation in this release.

## Responses

Ordinary supported values become JSON. flash::text emits text and flash::bytes
emits binary content. A void result yields 204. flash::created<T> carries a body
and Location. flash::response<T> carries a status, owned headers, and JSON body.
flash::response_message gives direct access to the response representation.

std::expected<T, Enum> uses the enum specialization of flash::error_map. Build
the mapping with flash::errors and flash::map. Every enum value requires exactly
one mapping. Binding, routing, and domain failures use problem JSON.

## Runtime and middleware

serve blocks until the server exits. It accepts server_config, optional stop
token, application middleware, and referenced state objects. raw_server exposes
start, stop, wait, port, and active_sessions for embedded control. State and
handlers must remain alive until all requests finish.

flash::application composes middleware in declaration order. Built in middleware
provides request IDs, an access log callback, and exception recovery. Request IDs
and logs allocate owned data. Callbacks run on request workers and must remain
short. Shared callback state needs synchronization.

Custom middleware accepts request_context and a callable for the next stage,
and returns flash::task<flash::response_message>. The request context exposes the
request view, request ID, and cooperative stop token.
