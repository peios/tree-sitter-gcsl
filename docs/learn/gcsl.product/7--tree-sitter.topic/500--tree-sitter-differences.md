---
title: Parser Differences
type: reference
description: Deliberate differences between the editor grammar and the GCSL compiler.
---

Use the compiler or GCSL language server to validate a program. The editor
grammar favors useful syntax trees while a file is incomplete. A successful
Tree-sitter parse does not establish that the compiler accepts the file.

## Accepted forms with additional compiler rules

The grammar accepts these broader forms:

| Area | Editor grammar behavior |
| --- | --- |
| Comparisons and ranges | Chained forms such as `a < b < c` and `a..b..c` associate left |
| Negation | A space can separate `-` from its operand |
| Paths | Shared path rules accept index segments in declarations and fields, quoted first names, and dotted mutable declarations |
| Quoted field names | Interpolation can appear where the compiler expects a plain string |
| Attributes | Attribute names and placement are not checked as strictly as by the compiler |
| Definition references | A def-type can be a field access on a computed expression |
| Definition paths | Some glued numeric and hyphenated segments are accepted more broadly |
| Loop headers | The grammar does not enforce every condition and declaration ordering rule |
| Newlines | A newline can be skipped before a token the compiler expects on the same line |
| Labels | A record-like block can carry a label |
| Escapes | Unknown escapes and a lone closing brace in string content can parse |
| Imports | Dotted members and repeated wildcard imports can parse |

These are editor parsing choices, not alternative GCSL language rules.
The compiler remains responsible for rejecting invalid source.

## Scanner boundaries

The external scanner stores at most 64 simultaneously open string literals.
Nested interpolation can reach this limit. The scanner stops opening more
literals and lets Tree-sitter recover; it does not allocate an unbounded
stack. This editor limit is separate from the compiler's depth limits.

String delimiter lengths are stored as 32-bit counts, including across
incremental edits. Delimiters longer than 255 quotes are supported.
The byte counts in Tree-sitter's input interface bound the document size.
The compatibility tests and known limitations should be reviewed whenever
the language's lexer or parser changes.

The grammar produces a concrete syntax tree with punctuation and recovery
nodes. Its tree shape is not the compiler's AST, even where node names are
shared. `@` is not source syntax; namespace keys created internally by the
compiler do not become valid source identifiers here.
