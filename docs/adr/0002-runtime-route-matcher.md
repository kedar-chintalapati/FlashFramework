# ADR 0002. Runtime route matcher

Status is an accepted workaround.

Date is 2026-09-03.

## Context

The route parser produces correct segments during constant evaluation. GCC 16.2
produced an incorrect result when a constexpr runtime matcher received an inline
route object derived from annotations. Unrelated observable work changed the
result. This indicates a compiler problem.

## Decision

Route grammar and API validation use constant evaluation. Request path comparison
uses an ordinary inline function because request paths are runtime data.

## Results

- Runtime performs the byte comparisons required for each request.
- Route normalization and conflict checks remain compile time work.
- The workaround can be tested again after a GCC upgrade.
- A small compiler reproduction should be kept if one can be isolated.
