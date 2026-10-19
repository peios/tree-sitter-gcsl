---
title: Tree-sitter Grammar
type: concept
description: How the GCSL editor grammar provides syntax trees and editor queries.
---

The GCSL Tree-sitter grammar lets an editor recognize the structure of a
GCSL file as you type. It supplies syntax highlighting and the structural
information an editor can use for folding, indentation, and selections.
The grammar does not run the program or check its types.

## Parser and queries

The parser turns source into a concrete syntax tree. Named nodes identify
constructs such as a function, an import, or a string interpolation. The
tree also records punctuation and source positions so an editor can work
with incomplete text and update a changed region.

Queries attach names to selected nodes. The editor interprets those names:
for example, a function capture can select a highlight group or define a
text object. [Editor Queries](~gcsl/tree-sitter-queries) describes the four
query files shipped with the grammar.

## Native and WebAssembly parsers

The repository builds two forms of the same parser. A native shared object
is loaded by an editor that accepts C Tree-sitter grammars. A WebAssembly
module is loaded by an editor with Tree-sitter's Wasm runtime. Both export
the language entry point `tree_sitter_gcsl` and use language ABI 15.

These artifacts are grammar plugins. They do not embed the GCSL evaluator
or a separate copy of the Tree-sitter runtime. The native parser and Wasm
parser are tested against Tree-sitter 0.27.1.

## Relationship to the language server

The GCSL compiler owns the language rules. Its language server provides
type errors, name resolution, navigation, and formatting. The Tree-sitter
grammar is a separate editor component and can work without the language
server.

The syntax tree is not the compiler's abstract syntax tree. Some node names
are similar, but their structure and meaning are not interchangeable.
The grammar deliberately accepts some text the compiler rejects and has
its own scanner limits. See
[Parser Differences](~gcsl/tree-sitter-differences) before using it to
judge whether a program is valid.
