# Building Tree-sitter GCSL

Use pekit to build the native and Wasm parsers and to run their tests. The
supported development host is x86-64 Linux.

## Before you start

You need pekit and Git. Everything else is pinned and fetched by pekit on
the first build: the Bootlin C toolchain, Python 3.14, busybox and GNU
make, Tree-sitter 0.27.1's CLI and runtime source, WASI SDK 34.0, Binaryen
132, and ncurses 6.5, whose terminal library WASI SDK's compiler needs.
The first build needs network access; later builds find what they need in
pekit's store, and every step runs without network access in a sandbox
that holds only its declared inputs.

The build files use the gcslib libraries named in `workspace.gcsl` and the
build files. `pekit lock` pins their commits in `pekit.lock`.

## Build

```sh
pekit build parser wasm
```

| Target | Output |
|---|---|
| `parser` | `gcsl.so`, the native parser, a Tree-sitter plugin |
| `wasm` | `gcsl.wasm`, the portable parser |
| `generated` | The parser files the pinned CLI generates from `grammar.js` |

Each output is under `pekit-out/<target>/`, and pekit prints its path
indented under the target's name. The native parser exports
`tree_sitter_gcsl` alone and is linked with full RELRO, immediate binding,
and a non-executable stack.

## Test

```sh
pekit test
```

| Test | Check |
|---|---|
| `generated` | The committed generated files match what the CLI generates |
| `corpus` | The corpus and highlight tests, against the native parser |
| `wasm_corpus` | The corpus and highlight tests, against the Wasm parser |
| `runtime` | Incremental edits and the queries, with Tree-sitter's runtime |
| `scanner` | The external scanner's state, compiled with warnings as errors |

The corpus tests load the built parser from a private directory with
compiling turned off, so the parser tested is the one built. A test whose
inputs did not change is found in the store and not run again.

The grammar uses ABI 15. Generated files remain committed for editor
consumers.

## Regeneration

Regenerate with the pinned CLI and its embedded JavaScript runtime:

```sh
tools/generate
```

The files are generated in a pekit sandbox, and each one that differs
replaces its committed copy in `src/`. Do not format `src/parser.c`, the
generated JSON, or `src/tree_sitter/` by hand. A pure formatting change to
`grammar.js` should leave those files byte-identical.

## Style and documentation

Install the pinned PSC tools into a directory outside the repository, then
run the shared entry point:

```sh
python3 tools/prepare-style-tools.py "$HOME/.cache/grammar-style"
export PEIOS_STYLE_BIN="$HOME/.cache/grammar-style/bin"
tools/check
TRAIL=/path/to/trail tools/check-docs
```

`tools/check` checks owned source formatting and lint with Ruff,
clang-format, ShellCheck, shfmt, and Prettier, then runs `pekit test`.
GitHub's workflow runs the style checks alone; builds and tests run with
pekit on the build server. The maintained code uses PSC. Generated and
pinned third-party code keeps its original formatting, as described in
`coding-style.md`.

`test/workspace.gcsl` makes `test/` a workspace of its own, so that the
repository's workspace leaves out the highlight fixtures in
`test/highlight/`, which are GCSL examples, not build files.

## Not built with pekit yet

Packages, debug information, and builds with the native Peios toolchain
are not yet built with pekit. The package definitions and the native build
settings from the Bazel build are kept outside this repository until they
are.
