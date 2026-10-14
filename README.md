# tree-sitter-gcsl

A [tree-sitter](https://tree-sitter.github.io) grammar for GCSL, with
highlight queries in nvim-treesitter's capture names. The authority for the
language is the gcsl crate (`crates/gcsl/src/lexer.rs` and `parser.rs`); the
grammar names its nodes after `ast.rs` where that reads well.

## Building

```sh
tree-sitter generate          # grammar.js -> src/parser.c, src/grammar.json, src/node-types.json
tree-sitter test              # test/corpus/*.txt and test/highlight/*.gcsl
tree-sitter build --wasm      # -> tree-sitter-gcsl.wasm
```

`tree-sitter build --wasm` downloads wasi-sdk the first time if neither
Emscripten nor Docker is set up. The generated `src/` is committed, so editors
that compile `src/parser.c` themselves need no tree-sitter CLI.

`src/scanner.c` is an external scanner, for two things a tree-sitter lexer
cannot do:

- **The newline that ends an item.** A `_newline` token is valid only where an
  item can end, so a newline anywhere else (inside `(` or `[`, directly in an
  interpolation, after an operator) is skipped as whitespace, which is gcsl's
  rule. Where it is valid, a newline is one unless the next word, past blank
  lines and comments, is `else`, which an `if` may start the next line with.
  That needs a look past any number of lines.
- **String literals**, delimited by `n` quotes each side for any `n` of 1 or
  at least 3, inside which a run of fewer than `n` quotes is text, and which
  nest through interpolations. The scanner keeps a fixed-size stack of the
  open literals' delimiters (64 deep, `n` up to 255).

## Divergences from gcsl's parser

The grammar accepts everything gcsl's parser accepts, with the same tree
shape. Where gcsl enforces a context-sensitive rule that would complicate the
grammar a lot, the grammar is looser and accepts more:

- **Chained comparisons and ranges.** `a < b < c`, `a == b is Int`,
  `a..b..c` parse, left-associative; gcsl rejects them.
- **Negation with a space.** `- x` where an operand starts parses as
  negation; gcsl's lexer makes a `-` followed by whitespace subtraction, an
  error there. (Subtraction does need the space, as in gcsl: `a -b` is an
  error.)
- **Paths.** `let`, field and rebind paths share one rule, so index segments
  in a `let` or a field path (`let xs[0] = 1`), a string as the first name of
  a `let`'s or a rebind's path, and a dotted `let mut` path parse.
- **Field names.** Where gcsl wants a plain string literal as a name, any
  string literal parses, raw or interpolated.
- **Attributes.** Any attribute name parses before any top-level item, and
  `#![no_prelude]` anywhere among a file's items. Where an attribute cannot
  stand (in a block, or after an item on its line), `#[x]` reads as a comment.
- **Def-type references.** A reference's def-type may be any field access
  (`f(x).y z`), not only a name or a chain of field accesses on one; and
  inside brackets, its path may start on the next line (`f(a` newline `b)`),
  where gcsl wants the source's same line.
- **Definition paths.** The run glued after a segment's `+` is read as one
  token of letters, digits, `_`, `+` and `-`, so a `-` gcsl's lexer rejects
  inside it (`c++11-x`, a `-` directly after the number `11`) parses.
- **`for` headers.** Any number of conditions, none included, a `let` after
  the condition, and a block expression after a header's first token parse.
- **Newlines before an expected token.** A newline is skipped wherever an
  item cannot end, so one before an `if`'s or a loop's `{`, before a `for`'s
  `in`, before the `for` of a `do`, or between a `let`'s path and its `=`
  parses; gcsl reads a newline there as the error it is.
- **Labels.** A record block (one with a field or a spread item) may be
  labelled.
- **Strings.** Escapes are not checked (`\q`, `\u{zz}` parse), and a lone `}`
  in a literal's text is text.
- **Imports.** A dotted member (`import a.b from p`) and a second `*` parse,
  and `from` straight after `import` is read as a module's name.

`@` is not GCSL syntax (it appears only in the namespace keys gcsl builds
internally) and is an error here as in gcsl.

## Highlighting

`queries/highlights.scm` uses nvim-treesitter's capture names. Editors differ
on which of two patterns capturing the same node wins (clackatoo the first,
the tree-sitter CLI and Neovim the last), so the query captures no node
under two names, but for one field access called as a function, whose
pattern stands on both sides of the general one. The effect markers `:`,
`->>` and `->` of functions are `@keyword.modifier`. GCSL has no methods: a
called field, `ppk.commit(...)`, is `@function.call`. A field item at a
file's top level (`meta = {..}`) names a `@variable`, one in a body a
`@variable.member`. The segments of a definition path are `@module`, but the
last one, the definition's name (`libcap` in `def package
org.kernel.libcap(v)`, `defmeta package org.kernel.libcap` and the
reference `build org.kernel.libcap`), is `@function`, as a `def`'s last name
is.

## Indents, folds and text objects

- `queries/indents.scm`, in nvim-treesitter's indent captures, indents by
  `shiftwidth` a level (4 in GCSL). Blocks, record types, lists, argument and
  parameter lists and parentheses open a level, and a line starting with the
  closing bracket goes back one. The blocks of an `if` open none of their
  own: the `if` opens one level for all of them, and an `else` is a branch
  of it, so an `else` starting a line lines up with the `if`, and
  `if c { a } else {` on one line opens one level. The lines of a string
  after its first (a `"""` or raw string's script), and comments, are
  `@indent.auto`: their indent is left as it is.
- `queries/folds.scm` (`@fold`): blocks, record types, lists, functions,
  definitions and metas, and runs of imports. Only nodes of several lines
  fold.
- `queries/textobjects.scm`, in nvim-treesitter-textobjects' names:
  `@function` (`def` items, named functions, function expressions,
  definitions), `@class` (`type` items, record literals, metas; not the body
  of a function, a definition or a meta), `@parameter` (parameters, arguments, list
  elements, the fields of record types and literals, the outer with its
  comma or `;`), `@block`, `@conditional`, `@loop` (`for`, `do`), `@call`,
  `@comment`, `@return` (`return`, and a block's `= EXPR`), `@assignment`
  (`let`, field items, rebinds; the inner is the value) and `@attribute`.
  Each `.inner` is the inside of the braces or brackets. The query uses no
  `#make-range!` and no predicates, so that clackatoo reads it as Neovim
  does: an object of several nodes is captured on each of them.

A record literal is a block whose first or last item is a field or a spread,
standing anywhere but as the body of a function, a definition or a meta.

`indents.scm` uses `#not-has-parent?`, which Neovim (built in; checked with
0.11) and clackatoo both read. `tree-sitter test` runs no tests of these queries; they
were checked against the newpkgs tree with Neovim's own indenter, folds and
nvim-treesitter-textobjects.

## Neovim (nvim-treesitter, master branch)

```lua
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()
parser_config.gcsl = {
  install_info = {
    url = "/path/to/tree-sitter-gcsl", -- or the repository's git URL
    files = { "src/parser.c", "src/scanner.c" },
    branch = "main",
  },
  filetype = "gcsl",
}
vim.filetype.add({ extension = { gcsl = "gcsl" } })
```

Then `:TSInstall gcsl`, and copy or link each of `queries/highlights.scm`,
`indents.scm`, `folds.scm` and `textobjects.scm` into `queries/gcsl/` in a
runtime path directory, such as `~/.config/nvim/queries/gcsl/`. Each then
needs turning on:

- indents: `indent = { enable = true }` in nvim-treesitter's setup (for
  `gcsl` at least), which sets `indentexpr`;
- folds: `foldmethod=expr` and `foldexpr=v:lua.vim.treesitter.foldexpr()`
  for gcsl buffers;
- text objects: nvim-treesitter-textobjects, with keymaps for the objects
  wanted (on its `main` branch, set with its own `select_textobject`).

## clackatoo

A language pack is a directory in `~/.local/share/clackatoo/languages`, say
`gcsl/`, holding:

- `language.toml`:

  ```toml
  grammar = "gcsl"
  extensions = ["gcsl"]
  filenames = []
  ```

- `grammar.wasm`: `tree-sitter-gcsl.wasm` from `tree-sitter build --wasm`;
- `highlights.scm`: `queries/highlights.scm`;
- `indents.scm`: `queries/indents.scm`, for Enter, `o` and `=`;
- `textobjects.scm`: `queries/textobjects.scm`, of which clackatoo reads
  `@function`, `@class` and `@parameter` (`af`/`if`, `ac`/`ic`, `aa`/`ia`).

clackatoo reads no folds query, so `folds.scm` is for Neovim only.
