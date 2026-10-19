# Packaging Tree-sitter GCSL

`./bazel build //:packages` writes canonical PPK blobs and manifests under
`bazel-bin/packages.store`. The writer is the recorded, unchanged PPK
implementation in `vendor/ppk`; packaging does not introduce another
manifest format.

The family uses the existing grammar version `0.1.0` and package revision
`0.1.0-1`. It contains these packages:

| Package suffix | Payload |
| --- | --- |
| `tree-sitter-gcsl` | Native parser, node descriptions, and queries |
| `tree-sitter-gcsl-wasm` | Portable Wasm parser and queries |
| `tree-sitter-gcsl-debuginfo` | Separate native debug information |
| `tree-sitter-gcsl-debugsource` | C source and headers for native debugging |
| `tree-sitter-gcsl-source` | Declared repository inputs for rebuilding |

Each name has the prefix `dev.peios.`. The native plugin installs as
`usr/lib/x86_64-linux-peios/tree-sitter/gcsl.so`; this is a Tree-sitter
plugin, not a new general-purpose `libgcsl` API. The Wasm plugin installs
as `usr/share/tree-sitter/grammars/gcsl.wasm`. Queries use
`usr/share/tree-sitter/queries/gcsl/` and are identical in both packages.

The family index records build profile, input identities, package metadata,
and licence information alongside the canonical manifests. A Linux
development build is labelled as such. Native builds validate IBT, shadow
stack, immediate binding, RELRO, and a nonexecutable stack before emitting
packages. Debug information is split from the native parser.

All outputs remain unsigned and marked `release_ready: false`. No release
tag, repository upload, package installation, or editor configuration change
is performed by the build. The package checks inspect the emitted payloads
and decode every referenced blob to verify its uncompressed content hash.
