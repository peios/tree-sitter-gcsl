# Building Tree-sitter GCSL

The supported development host is x86_64 Linux with a C compiler, binutils,
Python 3.11 or later, and the shell tools used by the Bazel launcher.
Bazel 8.4.2, Tree-sitter 0.27.1, WASI SDK 34.0, Binaryen 132, and the Rust
toolchain used only by the package writer are pinned in the repository.

Run the declared build and tests:

```sh
./bazel test //...
./bazel build //:gcsl.so //:wasm //:packages
```

The grammar uses ABI 15. Generated files remain committed for editor
consumers. The `generated-test` target regenerates them in a disposable
directory and compares every generated file. Native and Wasm corpus tests
load the declared artifacts through a private loader directory with
automatic compilation disabled. The C tests cover scanner state,
incremental edits, and query captures.

## Regeneration

Regenerate with the pinned CLI and its embedded JavaScript runtime:

```sh
./bazel run //:generate
```

The command generates into a temporary directory before replacing changed
committed outputs. Do not format `src/parser.c`, the generated JSON, or
`src/tree_sitter/` by hand. A pure formatting change to `grammar.js` should
leave those files byte-identical.

## Style and documentation

Prepare style tools outside the checkout, then run the checks:

```sh
python3 tools/prepare-style-tools.py /path/to/style-tools
PEIOS_STYLE_BIN=/path/to/style-tools/bin tools/check
TRAIL=/path/to/trail tools/check-docs
```

Use actual absolute paths for the style-tool directory and Trail executable.
The maintained code uses PSC. Generated and pinned third-party code keeps
its original formatting, as described in `coding-style.md`.

## Native Peios builds

Acquire inputs from the final source tree while the network is available:

```sh
tools/offline-build acquire /path/to/frozen-inputs
```

Run the native build in a caller-provided isolated Peios userspace with its
C toolchain, Python, binutils, and hardened Rust 1.98.1 or later. Set
`PEIOS_RUSTC` to that environment's compiler. Keep the source lock identical
to the one recorded during acquisition:

```sh
tools/offline-build test /path/to/frozen-inputs /path/to/private-build
```

The native configuration does not provision or modify the host operating
system. It disables dependency downloads and uses a private vendor view.
Its packages require native hardening properties. A successful isolated
userspace test is distinct from testing an installed editor in a Peios
guest.
