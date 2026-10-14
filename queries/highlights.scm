; Highlights for GCSL, in nvim-treesitter capture names.
;
; Engines disagree on which of two patterns that capture the same node
; wins: clackatoo takes the first, the tree-sitter CLI (0.27) and Neovim the
; last. So no node here is captured under two names, but in one place: a
; field access called as a function, whose pattern stands on both sides of
; the general one for field accesses (see "Field accesses"). In particular
; there is no catch-all `(identifier) @variable`: the positions where an
; identifier is a variable are listed instead.

; ---- Comments and attributes ------------------------------------------------

(comment) @comment

[
  (attribute)
  (inner_attribute)
] @attribute

; ---- Literals ---------------------------------------------------------------

(string) @string

(raw_string) @string.special

(escape_sequence) @string.escape

(interpolation
  [
    "{"
    "}"
  ] @punctuation.special)

(integer) @number

(float) @number.float

(boolean) @boolean

(label) @label

; ---- Keywords ---------------------------------------------------------------

[
  "fn"
  "def"
  "defmeta"
] @keyword.function

; The def-type of a definition, a meta and a reference: `package` in
; `def package x.y(v)`, `defmeta package x.y` and `package x.y`.
(def_type
  (identifier) @keyword.function)

(reference
  def_type: (identifier) @keyword.function)

[
  "if"
  "else"
] @keyword.conditional

[
  "for"
  "do"
] @keyword.repeat

[
  "return"
  "break"
  "continue"
] @keyword.return

[
  "and"
  "or"
  "not"
  "is"
  "as"
  "in"
] @keyword.operator

[
  "import"
  "from"
] @keyword.import

[
  "let"
  "mut"
  "lazy"
  "await"
] @keyword.modifier

[
  "type"
  "META"
] @keyword

; The effect markers `:`, `->>` and `->` of functions and function types.
(effect) @keyword.modifier

; ---- Types ------------------------------------------------------------------

(type_declaration
  name: (identifier) @type.definition)

((type_identifier) @type.builtin
  (#any-of? @type.builtin "Int" "Float" "Bool" "String" "List" "Future" "Present"))

((type_identifier) @type
  (#not-any-of? @type "Int" "Float" "Bool" "String" "List" "Future" "Present"))

; `t.PackageDef`: the module, then the type.
(qualified_type
  (identifier) @module
  .
  (identifier))

(qualified_type
  (identifier) @type .)

; ---- Functions --------------------------------------------------------------

(named_function
  name: (identifier) @function)

; `def dev.peios.hello`: the module, then the function.
(public_function
  name: (dotted_name
    (identifier) @module
    .
    (identifier)))

(public_function
  name: (dotted_name
    (identifier) @function .))

; The prelude's functions (gcsl stdlib/prelude.gcsl).
(call
  function: (identifier) @function.builtin
  (#any-of? @function.builtin
    "len" "show" "int" "float" "half" "exec" "exec_live" "panic" "split" "join"
    "starts_with" "ends_with" "trim" "strip_prefix" "strip_suffix" "lines"
    "fields" "compare_versions" "sort_versions" "sh" "sh_live"))

(call
  function: (identifier) @function.call
  (#not-any-of? @function.call
    "len" "show" "int" "float" "half" "exec" "exec_live" "panic" "split" "join"
    "starts_with" "ends_with" "trim" "strip_prefix" "strip_suffix" "lines"
    "fields" "compare_versions" "sort_versions" "sh" "sh_live"))

; ---- Parameters -------------------------------------------------------------

(parameter
  name: (identifier) @variable.parameter)

(parameter_type
  name: (identifier) @variable.parameter)

(named_argument
  name: (identifier) @variable.parameter)

; ---- Modules: imports and definition paths ----------------------------------

(import
  path: (module_path
    (identifier) @module))

(import
  source: (module_path
    (identifier) @module))

(import
  alias: (identifier) @module)

(import_member
  path: (module_path
    (identifier) @variable))

(import_member
  alias: (identifier) @variable)

; A definition path's segments are modules, but the last segment of a
; definition's path, the name it defines, which is a function, as a `def`'s
; last name is: `libcap` in `def package org.kernel.libcap(v)`, and likewise
; the last segment of a meta's and a reference's path, which names that
; definition. Each segment matches one of these patterns only.
(definition_path
  (segment) @module
  .
  (segment))

(definition_path
  (segment
    (identifier) @module)
  .
  (segment))

(meta
  path: (definition_path
    (segment) @function .))

(meta
  path: (definition_path
    (segment
      (identifier) @function) .))

(reference
  path: (definition_path
    (segment) @function .))

(reference
  path: (definition_path
    (segment
      (identifier) @function) .))

(definition
  path: (definition_path
    (segment) @function .))

(definition
  path: (definition_path
    (segment
      (identifier) @function) .))

; ---- Variables and fields ---------------------------------------------------

; A `let` or a rebind names a variable, and so does a field item at a file's
; top level (`meta = {..}`); a field item in a body names a field. A path's
; later names are fields.
(let
  path: (path
    name: (identifier) @variable))

(rebind
  path: (path
    name: (identifier) @variable))

(source_file
  (field
    path: (path
      name: (identifier) @variable)))

(block
  (field
    path: (path
      name: (identifier) @variable.member)))

(path
  field: (identifier) @variable.member)

(field_type
  name: (identifier) @variable.member)

; An identifier where an expression stands, by the field that holds it...
(_
  left: (identifier) @variable)

(_
  right: (identifier) @variable)

(_
  operand: (identifier) @variable)

(_
  body: (identifier) @variable)

(_
  record: (identifier) @variable)

(_
  list: (identifier) @variable)

(_
  index: (identifier) @variable)

(_
  value: (identifier) @variable)

(_
  default: (identifier) @variable)

(_
  condition: (identifier) @variable)

(_
  iterable: (identifier) @variable)

(_
  guard: (identifier) @variable)

(_
  expression: (identifier) @variable)

(_
  variable: (identifier) @variable)

; ...or by the node it stands in.
(source_file
  (identifier) @variable)

(block
  (identifier) @variable)

(list
  (identifier) @variable)

(arguments
  (identifier) @variable)

(parenthesized_expression
  (identifier) @variable)

; Field accesses. A field access that is called, or that is a reference's
; def-type, is captured both before and after the general pattern, so that
; it wins whether the first pattern or the last wins.
(call
  function: (field_access
    field: (identifier) @function.call))

(reference
  def_type: (field_access
    field: (identifier) @keyword.function))

(field_access
  field: (identifier) @variable.member)

(call
  function: (field_access
    field: (identifier) @function.call))

(reference
  def_type: (field_access
    field: (identifier) @keyword.function))

; ---- Operators and punctuation ----------------------------------------------

(binary
  operator: [
    "+"
    "-"
    "*"
    "/"
    "//"
    "%"
    "=="
    "!="
    "==="
    "!=="
    "<"
    "<="
    ">"
    ">="
    ".."
    "..="
  ] @operator)

(unary
  operator: "-" @operator)

[
  "="
  ":="
  "..."
  "|"
  (wildcard)
] @operator

(applied_type
  [
    "<"
    ">"
  ] @punctuation.bracket)

(block
  [
    "{"
    "}"
  ] @punctuation.bracket)

(record_type
  [
    "{"
    "}"
  ] @punctuation.bracket)

(list
  [
    "["
    "]"
  ] @punctuation.bracket)

(index
  [
    "["
    "]"
  ] @punctuation.bracket)

(index_segment
  [
    "["
    "]"
  ] @punctuation.bracket)

(list_type
  "[]" @punctuation.bracket)

(arguments
  [
    "("
    ")"
  ] @punctuation.bracket)

(parameters
  [
    "("
    ")"
  ] @punctuation.bracket)

(parameter_types
  [
    "("
    ")"
  ] @punctuation.bracket)

(parenthesized_expression
  [
    "("
    ")"
  ] @punctuation.bracket)

(parenthesized_type
  [
    "("
    ")"
  ] @punctuation.bracket)

[
  ","
  ";"
  "."
] @punctuation.delimiter

(let
  ":" @punctuation.delimiter)

(field
  ":" @punctuation.delimiter)

(ascription
  ":" @punctuation.delimiter)

(parameter
  ":" @punctuation.delimiter)

(field_type
  ":" @punctuation.delimiter)

(parameter_type
  ":" @punctuation.delimiter)
