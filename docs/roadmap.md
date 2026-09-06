# Roadmap

The experimental release provides reflected typed endpoints over HTTP 1.1.
It has no stable API or ABI promise.

Next work includes a trie or shared matcher that supports 1,000 routes within the
compiler memory limit, equivalent baseline lifecycle costs, fewer response
allocations, global connection limits, sustained fuzzing, and broader compiler
validation. The current generated route report records the scaling limit.

Streaming, multipart bodies, TLS helpers, WebSockets, HTTP 2, and additional
JSON codec policies are deferred. A later preview requires stronger performance
evidence. A stable label requires compatibility policy and production evidence.
