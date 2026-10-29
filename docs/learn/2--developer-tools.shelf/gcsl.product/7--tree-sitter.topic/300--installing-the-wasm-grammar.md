---
title: Installing the Wasm Grammar
type: how-to
description: Assemble a GCSL language pack for clackatoo from the Wasm parser and queries.
---

The Wasm parser lets clackatoo recognize GCSL without loading a native
shared object into the editor. A language pack combines the parser, query
files, and a small language declaration.

## Before you start

You need a clackatoo version that supports Wasm language packs and the
grammar repository. To use the language server too, install `gcsl` on the
editor's command search path.

Build the portable parser from the grammar repository with pekit:

```sh
wasm=$(pekit build wasm | sed -n 's/^  //p')
```

The command keeps the built parser's path, which pekit prints indented
under the target name, in `wasm`.

## Create the language pack

1. Create the user language-pack directory:

   ```sh
   pack="${XDG_DATA_HOME:-$HOME/.local/share}/clackatoo/languages/gcsl"
   mkdir -p "$pack"
   ```

2. Copy the parser and supported queries:

   ```sh
   cp "$wasm" "$pack/grammar.wasm"
   cp queries/highlights.scm queries/indents.scm queries/textobjects.scm "$pack/"
   ```

3. Save this declaration as `language.toml` in that directory:

   ```toml
   grammar = "gcsl"
   extensions = ["gcsl"]
   filenames = []
   server = ["gcsl", "lsp"]
   roots = ["workspace.gcsl"]
   word = "-"
   ```

The `word` entry keeps names such as `build-root` together during word
navigation. The `server` entry starts the separate GCSL language server;
omit it if you only want grammar features. The root marker selects the
workspace for language-server requests.

## Check the result

Open a `.gcsl` file in clackatoo and check that strings and function names
are highlighted. Use a function text object to select a function. If the
language server is enabled, hover over a declaration to check that the
server starts separately from the grammar.

The pack does not include `folds.scm`: this clackatoo integration does not
consume the folding query. The same query remains available for editors
that support it.
