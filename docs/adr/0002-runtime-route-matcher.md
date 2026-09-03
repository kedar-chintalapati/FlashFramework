# ADR 0002: Keep runtime route matching out of constant evaluation on GCC 16.2

- Status: accepted workaround
- Date: 2026-09-03

## Context

The consteval route parser produces correct normalized segments and all direct comparisons succeed. During the route spike, GCC 16.2 generated an incorrect `false` result for a `constexpr` runtime matcher when its input was an inline route object derived from reflected annotations. Adding unrelated observable work changed the result back to `true`, indicating incorrect constant folding or code generation rather than a route-algorithm defect.

## Decision

Route grammar parsing and API validation remain consteval. The byte-oriented request-path matcher is an ordinary inline runtime function, which is semantically appropriate because the request path is runtime data and avoids the compiler defect.

## Consequences

- No routing decision is moved from compile time back to runtime beyond the byte comparisons that were always required.
- The normalized route, conflict checks, and specialized API traversal remain compile-time generated.
- The workaround can be retested and potentially removed when the reference GCC patch revision changes.
- A minimal compiler reproduction should be retained if the issue can be reduced without Flash headers.

