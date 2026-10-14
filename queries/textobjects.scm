; Text objects for GCSL, in nvim-treesitter-textobjects' capture names.
;
; Written to read the same in Neovim (nvim-treesitter-textobjects) and in
; clackatoo, which takes `@function.*`, `@class.*` and `@parameter.*`. So
; no `#make-range!` and no predicates, which clackatoo doesn't read: where
; one object is several nodes, it is captured on each, and both take it from
; the first node's start to the last's end. The insides of braces are
; `"{" . (_) @x (_)? @x . "}"`: the first and the last item.

; ---- Functions --------------------------------------------------------------

; `def` items, named functions, function expressions, and definitions
; (`def DT PATH(..) {..}`). The inside is the body's items.
[
  (public_function)
  (named_function)
  (function)
  (definition)
] @function.outer

(public_function
  body: (block
    "{"
    .
    (_) @function.inner
    (_)? @function.inner
    .
    "}"))

(named_function
  body: (block
    "{"
    .
    (_) @function.inner
    (_)? @function.inner
    .
    "}"))

(function
  body: (block
    "{"
    .
    (_) @function.inner
    (_)? @function.inner
    .
    "}"))

(definition
  body: (block
    "{"
    .
    (_) @function.inner
    (_)? @function.inner
    .
    "}"))

; ---- Classes ----------------------------------------------------------------

; `type` items, record literals (a block with a field or a spread item) and
; metas. The body of a `def`, a named function, a function expression, a
; definition or a meta is not a class of its own, even with fields in it, so
; `ac` in `def package NAME(v) { .. }` goes out past the body. Without
; predicates, which clackatoo doesn't read, a record literal is found by
; where it stands instead: in any field of its parent but `body`, as a
; `for`'s, a `do`'s or a `lazy`'s body, or as an element, an argument, an
; item or in parentheses.
(type_declaration) @class.outer

(type_declaration
  type: (record_type
    "{"
    .
    (_) @class.inner
    (_)? @class.inner
    .
    "}"))

(type_declaration
  type: [
    (type_identifier)
    (qualified_type)
    (applied_type)
    (function_type)
    (list_type)
    (union_type)
    (parenthesized_type)
  ] @class.inner)

; A record literal is a block whose first item is a field or a spread (the
; first group), or whose last is (the second); its inside is from its first
; item to its last. (A wildcard parent with `!body` would be shorter, but
; tree-sitter 0.27 matches a definition's body with it all the same.)
(_
  [
    value: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    left: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    right: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    operand: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    function: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    record: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    list: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    index: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    condition: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    iterable: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    guard: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    default: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    expression: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    consequence: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    alternative: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
    block: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer
  ])

[
  (arguments (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (list (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (parenthesized_expression (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (block (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (source_file (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (for body: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (do body: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
  (lazy body: (block "{" . [(field) (spread)] @class.inner (_)? @class.inner . "}") @class.outer)
]

(_
  [
    value: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    left: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    right: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    operand: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    function: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    record: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    list: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    index: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    condition: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    iterable: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    guard: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    default: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    expression: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    consequence: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    alternative: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
    block: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer
  ])

[
  (arguments (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (list (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (parenthesized_expression (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (block (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (source_file (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (for body: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (do body: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
  (lazy body: (block "{" . (_) @class.inner [(field) (spread)] @class.inner . "}") @class.outer)
]

(meta) @class.outer

(meta
  body: (block
    "{"
    .
    (_) @class.inner
    (_)? @class.inner
    .
    "}"))

(meta
  value: (_) @class.inner)

; ---- Parameters -------------------------------------------------------------

; Parameters, arguments (named or not), list elements, and the fields of
; record literals and record types. The inside is the element; the outside
; takes its comma with it: the one after it, or for the last element with
; no comma after it, the one before it. The kinds of an element are listed,
; not `(_)`, which would take a comment among them as one.
(parameters
  (parameter) @parameter.inner)

(parameters
  (parameter) @parameter.outer
  .
  "," @parameter.outer)

(parameters
  "," @parameter.outer
  .
  (parameter) @parameter.outer
  .
  ")")

(parameters
  "("
  .
  (parameter) @parameter.outer
  .
  ")")

(parameter_types
  (parameter_type) @parameter.inner)

(parameter_types
  (parameter_type) @parameter.outer
  .
  "," @parameter.outer)

(parameter_types
  "," @parameter.outer
  .
  (parameter_type) @parameter.outer
  .
  ")")

(parameter_types
  "("
  .
  (parameter_type) @parameter.outer
  .
  ")")

(arguments
  [
    (named_argument)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.inner)

(arguments
  [
    (named_argument)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  "," @parameter.outer)

(arguments
  ","  @parameter.outer
  .
  [
    (named_argument)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  ")")

(arguments
  "("
  .
  [
    (named_argument)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  ")")

(list
  [
    (spread)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.inner)

(list
  [
    (spread)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  "," @parameter.outer)

(list
  "," @parameter.outer
  .
  [
    (spread)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  "]")

(list
  "["
  .
  [
    (spread)
    (binary)
    (unary)
    (lazy)
    (call)
    (field_access)
    (index)
    (ascription)
    (narrowing)
    (type_test)
    (reference)
    (identifier)
    (integer)
    (float)
    (boolean)
    (string)
    (raw_string)
    (parenthesized_expression)
    (list)
    (block)
    (if)
    (for)
    (do)
    (function)
  ] @parameter.outer
  .
  "]")

(record_type
  [
    (field_type)
    (spread)
  ] @parameter.inner)

(record_type
  [
    (field_type)
    (spread)
  ] @parameter.outer
  .
  "," @parameter.outer)

(record_type
  "," @parameter.outer
  .
  [
    (field_type)
    (spread)
  ] @parameter.outer
  .
  "}")

(record_type
  "{"
  .
  [
    (field_type)
    (spread)
  ] @parameter.outer
  .
  "}")

; A record literal's fields are separated by newlines or `;`: the outside
; takes a `;` after it.
(block
  [
    (field)
    (spread)
  ] @parameter.inner @parameter.outer
  .
  ";"? @parameter.outer)

; ---- Blocks, conditionals, loops and calls ----------------------------------

(block) @block.outer

(block
  "{"
  .
  (_) @block.inner
  (_)? @block.inner
  .
  "}")

(if) @conditional.outer

(if
  consequence: (block
    "{"
    .
    (_) @conditional.inner
    (_)? @conditional.inner
    .
    "}"))

(if
  alternative: (block
    "{"
    .
    (_) @conditional.inner
    (_)? @conditional.inner
    .
    "}"))

[
  (for)
  (do)
] @loop.outer

(for
  body: (block
    "{"
    .
    (_) @loop.inner
    (_)? @loop.inner
    .
    "}"))

(do
  body: (block
    "{"
    .
    (_) @loop.inner
    (_)? @loop.inner
    .
    "}"))

(call) @call.outer

(call
  arguments: (arguments
    "("
    .
    (_) @call.inner
    (_)? @call.inner
    .
    ")"))

; A call with a trailing block and no arguments, `f { .. }`: the block's
; items.
(call
  !arguments
  block: (block
    "{"
    .
    (_) @call.inner
    (_)? @call.inner
    .
    "}"))

; ---- Items ------------------------------------------------------------------

(comment) @comment.outer

; `return`, and a block's result, `= EXPR`.
[
  (return)
  (result)
] @return.outer

; `let`, field items and rebinds; the inside is the value.
(let
  value: (_) @assignment.inner) @assignment.outer

(field
  value: (_) @assignment.inner) @assignment.outer

(rebind
  value: (_) @assignment.inner) @assignment.outer

[
  (attribute)
  (inner_attribute)
] @attribute.outer
