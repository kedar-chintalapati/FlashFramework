# Changelog

## 0.1.0

This is the first experimental release.

It includes reflected endpoint discovery, compile time route checks, typed input,
native JSON, shared validation and OpenAPI metadata, domain errors, application
state, static middleware, Asio handlers, a bounded Beast server, development
documentation routes, package installation, examples, and benchmark tools.

Windows Debug and Release tests and Linux sanitizer tests run in CI with GCC
16.2.0. The initial Windows request matrix completed without request errors.

The release does not make a production, compatibility, or general zero allocation
claim. Several throughput cases missed the proposed target. The measured build
time missed its proposed budget. A 1,000 route generated target crossed the
compiler memory guard. Pattern checks, custom validators, streaming, TLS helpers,
multipart bodies, WebSockets, and HTTP 2 are deferred. The benchmark reports and
roadmap contain the measured values and follow up work.
