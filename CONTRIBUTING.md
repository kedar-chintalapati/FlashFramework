# Contributing

Use GCC 16.2.0 and the pinned dependency setup in docs/build.md. Keep changes
focused and include a reproduction or test for behavioral changes. Run the
relevant Debug and Release tests. New compile diagnostics should have a compile
failure test checking a stable Flash identifier.

Keep public examples small. Document allocation boundaries and benchmark
conditions when making performance claims. Avoid committing generated build
output or machine specific files. Use two local build jobs unless you have
verified that your machine can safely handle more.

Open an issue for a bug or a substantial API proposal. Report security issues
through the process in SECURITY.md. Contributions are accepted under the MIT
license of this repository.
