---
title: Editor Queries
type: reference
description: The GCSL highlight, indentation, folding, and text-object query contracts.
---

The query files use Tree-sitter node patterns and capture names understood
by editor integrations. Keep the parser and queries from the same grammar
revision: a query referring to a missing node cannot be loaded.

The files have distinct roles:

| File | Purpose | Required editor behavior |
| --- | --- | --- |
| `highlights.scm` | Syntax colors | Map captures to highlight groups and evaluate predicates |
| `indents.scm` | Structural indentation | Interpret `indent.*` captures and `#not-has-parent?` |
| `folds.scm` | Fold ranges | Interpret `@fold` on multiline constructs |
| `textobjects.scm` | Structural selections | Combine captures in one match into an inner or outer range |

## Highlights

Top-level field names use `@variable`; fields inside bodies use
`@variable.member`. A called field uses `@function.call`, because GCSL has
function values rather than a separate method-call syntax. The final name
in a definition path uses `@function`, while preceding segments use
`@module`. Function effect markers use `@keyword.modifier`.

Editors differ in which overlapping highlight pattern wins. The queries
avoid most competing names and repeat specialized field-call patterns on
both sides of the general field pattern. This supports integrations that
choose either the first or last applicable capture.

## Indentation

Blocks, lists, argument lists, and parenthesized constructs open indentation
levels. An `if` opens a level for its branches; the branch blocks do not
open an extra level. A continued binary expression opens a hanging level
only at the outermost binary node.

The editor chooses the width of a level. GCSL's house style is four spaces.
String content and comments use `@indent.auto` so structural indentation
does not rewrite literal text. The predicate `#not-has-parent?` is an editor
extension; a generic query engine alone does not implement its behavior.

## Folds and selections

Folds cover blocks, record types, lists, functions, definitions, metas, and
runs of imports. A one-line node does not need a multiline fold.

Text objects cover functions, classes, parameters, blocks, conditionals,
loops, calls, comments, returns, assignments, and attributes. The `.inner`
capture selects the contents and `.outer` includes the surrounding
construct. Parameter outer ranges include an adjacent separator where one
is present.

The text-object query uses captures for the first and last selected nodes
rather than `#make-range!`. The editor combines those captures. It treats
a block as a record-like class only in the contexts described by the query;
it does not perform the compiler's type analysis.
