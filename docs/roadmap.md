# Roadmap

The experimental release provides reflected typed endpoints over HTTP 1.1.
It has no stable API or ABI promise.

Next work includes generated route scaling, equivalent baseline lifecycle costs,
fewer response allocations, global connection limits, sustained fuzzing, and
broader compiler validation. Larger synthetic API build measurements remain
necessary before promising practical support for thousands of endpoints.

Streaming, multipart bodies, TLS helpers, WebSockets, HTTP 2, and additional
JSON codec policies are deferred. A later preview requires stronger performance
evidence. A stable label requires compatibility policy and production evidence.
