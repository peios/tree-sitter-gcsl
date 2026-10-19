; Indents for GCSL, in nvim-treesitter's indent captures, which clackatoo
; reads as Neovim does. GCSL is indented by 4 spaces a level; the width is the
; editor's `shiftwidth`.
;
; A line is indented a level for each bracketed node it is inside that
; started on an earlier line, at most once for the nodes starting on any one
; line, so `f(a, {` opens one level, not two.

; ---- Brackets ---------------------------------------------------------------

; Blocks (bodies, record literals), record types, lists, argument and
; parameter lists, and parentheses open a level. The blocks of an `if` are
; the exception: the `if` opens their level (see "if and else").
[
    (record_type)
    (list)
    (arguments)
    (parameters)
    (parameter_types)
    (parenthesized_expression)
    (parenthesized_type)
] @indent.begin

((block) @indent.begin
    (#not-has-parent? @indent.begin if))

; A binary expression continued on later lines, `"a"` newline `+ "b"`,
; indents them a level (a hanging indent). Only the outermost of a chain
; opens it, its operands starting on the line it does.
((binary) @indent.begin
    (#not-has-parent? @indent.begin binary))

; A line starting with the closing bracket goes back a level (a branch, as
; nvim-treesitter calls it), and a new line after a line ending with one is
; indented as the line the node started on (`@indent.end`).
(block
    "}" @indent.branch @indent.end)

(record_type
    "}" @indent.branch @indent.end)

(list
    "]" @indent.branch @indent.end)

(arguments
    ")" @indent.branch @indent.end)

(parameters
    ")" @indent.branch @indent.end)

(parameter_types
    ")" @indent.branch @indent.end)

(parenthesized_expression
    ")" @indent.branch @indent.end)

(parenthesized_type
    ")" @indent.branch @indent.end)

; ---- if and else ------------------------------------------------------------

; The whole `if`, with every `else` after it, opens one level, from the line
; it starts on; its blocks, and an `if` after an `else`, open none of their
; own. An `else`, which may start a line, is a branch of it, back at the
; `if`'s indent, as is the `}` before it:
;
;     if a {
;         x
;     }
;     else if b {
;         y
;     } else {
;         z
;     }
;
; So the lines of an `else`'s block are a level in from the line the `if`
; starts on, whichever line the `else` is on: `if c { a } else {` on one line,
; and `f(x, if c {` then `} else {`, where the `if`'s line opens one level
; for the call and the `if` together. (An `if`'s parent is an `if` only when
; it follows an `else`.)
((if) @indent.begin
    (#not-has-parent? @indent.begin if))

(if
    "else" @indent.branch)

; ---- Text kept as it is -----------------------------------------------------

; A string's lines after its first, including the line of its closing
; quotes, keep the indent they have: the text of a `"""` or raw string is
; its own (a shell script, say), and an indent there is part of it.
; `@indent.auto` keeps a line's indent; `@indent.ignore` would put it at
; the margin.
[
    (string)
    (raw_string)
] @indent.auto

(comment) @indent.auto
