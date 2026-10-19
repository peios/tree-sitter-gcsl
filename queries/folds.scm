; Folds for GCSL, in nvim-treesitter's `@fold`. Only a node of several lines
; folds, so a list or a block on one line makes no fold.

; Blocks: function bodies, record literals, and the bodies of `if`, `for`
; and `do`. Record types and lists.
[
    (block)
    (record_type)
    (list)
] @fold

; Functions, definitions and metas, from their first line: a header that
; spans lines folds with the body.
[
    (named_function)
    (public_function)
    (function)
    (definition)
    (meta)
] @fold

; A run of imports, one after another.
(import)+ @fold
