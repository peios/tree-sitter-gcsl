# Tree-sitter GCSL

An incremental GCSL editor grammar with native and WebAssembly parsers,
highlighting, indentation, folding, and text-object queries. The GCSL
compiler remains the language authority; this parser is not a validator or
evaluator.

Build and test on x86_64 Linux:

```sh
./bazel test //...
./bazel build //:gcsl.so //:wasm //:packages
```

The native parser is `bazel-bin/gcsl.so`, the portable parser is
`bazel-bin/tree-sitter-gcsl.wasm`, and the unsigned development PPK store is
`bazel-bin/packages.store`.

See [building](docs/building.md), [packaging](docs/packaging.md), and
[coding style](docs/coding-style.md) for repository maintenance. The
[Learn contribution](docs/learn/gcsl.product/7--tree-sitter.topic/) explains
editor setup, query behavior, and parser differences. Panorama merges it
into the GCSL documentation product.

The generated C and node descriptions remain in `src/` for editors that
compile grammars directly. Keep them synchronized with `grammar.js` using
the pinned generator. Peios-authored files are MIT licensed; see [LICENSE](LICENSE).
Third-party notices are recorded under `docs/licenses/`.
