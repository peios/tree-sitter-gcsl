---
title: Installing GCSL Parsers
type: how-to
description: Load the native GCSL parser and queries in Neovim.
---

You can load the native parser directly in Neovim without a parser-manager
plugin. This procedure configures syntax highlighting and folds. It does
not configure the language server or install an indentation plugin.

## Before you start

You need the following:

- Neovim 0.11 with support for Tree-sitter language ABI 15.
- The grammar's `gcsl.so` native build for your host.
- The four files in the grammar repository's `queries/` directory.

The example uses a Linux development build. From the grammar repository,
build the native parser:

```sh
./bazel build //:gcsl.so
```

## Install the parser and queries

1. Create the runtime directories under your Neovim configuration:

   ```sh
   config="${XDG_CONFIG_HOME:-$HOME/.config}/nvim"
   mkdir -p "$config/parser" "$config/queries/gcsl"
   ```

2. Copy the parser and queries into those directories:

   ```sh
   cp bazel-bin/gcsl.so "$config/parser/gcsl.so"
   cp queries/*.scm "$config/queries/gcsl/"
   ```

3. Add the file type and highlighting setup to `init.lua`:

   ```lua
   vim.filetype.add({ extension = { gcsl = "gcsl" } })
   vim.api.nvim_create_autocmd("FileType", {
       pattern = "gcsl",
       callback = function()
           vim.treesitter.start(0, "gcsl")
           vim.wo.foldmethod = "expr"
           vim.wo.foldexpr = "v:lua.vim.treesitter.foldexpr()"
       end,
   })
   ```

## Check the result

Open a `.gcsl` file containing a function and a string interpolation. Run
`:InspectTree` to view the syntax tree. It should contain `named_function`
and `interpolation` nodes for those constructs.

Run `:Inspect` over a function name to see its highlight capture. Use `za`
on a multiline block to toggle its fold. A parser-load error usually means
the shared object was built for a different host or the editor does not
support its language ABI.

## Indentation and text objects

The indentation and text-object queries require an editor integration that
understands their capture names and predicates. Merely copying the files
does not enable those features. Configure your chosen integration using
its version's instructions; plugin APIs can differ between branches.
