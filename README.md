# Tree-sitter GCSL

An incremental GCSL editor grammar with native and WebAssembly parsers,
highlighting, indentation, folding, and text-object queries. The GCSL
compiler remains the language authority; this parser is not a validator or
evaluator.

Build and test on x86-64 Linux with pekit:

```sh
pekit test
pekit build parser wasm
```

The native parser is `gcsl.so` and the portable parser is `gcsl.wasm`; pekit
prints where each is, under `pekit-out/`.

See [building](docs/building.md) and [coding style](docs/coding-style.md)
for repository maintenance. The
[Learn contribution](docs/learn/2--developer-tools.shelf/gcsl.product/7--tree-sitter.topic/) explains
editor setup, query behavior, and parser differences. Peios Learn merges it
into the GCSL documentation product.

The generated C and node descriptions remain in `src/` for editors that
compile grammars directly. Keep them synchronized with `grammar.js` using
the pinned generator. Peios-authored files are MIT licensed; see [LICENSE](LICENSE).
Third-party notices are recorded under `docs/licenses/`.
