/**
 * @file A tree-sitter grammar for GCSL.
 *
 * The authority is the gcsl crate: the module docs of `lexer.rs` and
 * `parser.rs`, cited below by section ("parser: Items and bodies"), and the
 * node names of `ast.rs`. Where this grammar accepts more than gcsl's parser
 * does, README.md lists it under "Divergences".
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

// Binding powers, in the order of the parser docs' precedence table
// (parser: Expressions), loosest first. `lazy` heads the list: its operand
// is parsed at minimum binding power 0 (parser: Futures), as a `do`'s
// condition is (parser: Loops), so it takes everything after it.
const PREC = {
  lazy: -1,
  or: 1,
  and: 3,
  // Prefix `not`: its operand is parsed at minimum 5, so it takes in a
  // comparison but not `and` or `or`.
  not: 4,
  // `==` `!=` `===` `!==` `<` `<=` `>` `>=`, and postfix `is` TYPE.
  compare: 5,
  // `..` and `..=`.
  range: 6,
  additive: 7,
  multiplicative: 9,
  // Postfix `:` TYPE and `as` TYPE.
  ascription: 11,
  // Prefix `-` and `await`: operand parsed at minimum 13.
  unary: 13,
  // Postfix `.` NAME, call, index, and a def-type reference, which binds as
  // a field access does (parser: Def-types).
  postfix: 15,
};

// Within a type (parser: Types): a function type's return type takes a
// union after it, `|` binds looser than `T[]`, and a name's `<` or `.`
// always belongs to the type (an applied or a qualified one), never to a
// comparison or a field access after an ascription.
const TYPE_PREC = {
  function: 1,
  union: 2,
  list: 3,
  applied: 4,
};

// Where two readings of the same tokens conflict and gcsl always takes one,
// the rule for that one has this precedence: a `type` item over a reference
// whose def-type is a name `type`, and a `for`'s body over a block starting
// its header.
const PREFERRED = 1;

// An identifier (lexer): an ASCII letter or `_`, then letters, digits and
// `_`, and `-` when directly followed by one of those.
const IDENT = '[A-Za-z_]([A-Za-z0-9_]|-[A-Za-z0-9_])*';

const commaSep1 = (rule) => seq(rule, repeat(seq(',', rule)), optional(','));
const commaSep = (rule) => optional(commaSep1(rule));

/**
 * A `break`, `continue` or `return` item (parser: Loops).
 *
 * @param {GrammarSymbols<string>} $
 * @param {string} keyword
 */
const jump = ($, keyword) => seq(
  keyword,
  optional(field('label', $.label)),
  optional(field('value', $._expression)),
);

/**
 * The expression rules, in one of two variants.
 *
 * parser: Trailing blocks. While the parser is in the condition of an `if`,
 * the iterable or guard of a `for`, or a `for` header, a `{` after an
 * expression never starts a trailing block, at every operator of the
 * condition; the flag is lifted inside brackets, blocks and interpolations.
 * The `cond` variant is that flag: its rules are hidden copies (`_cond_*`)
 * of the ordinary ones, with no trailing block, aliased back to the
 * ordinary names so the tree is the same. Everything bracketed inside them
 * (arguments, indexes, parentheses, lists, blocks) is an ordinary
 * expression again.
 *
 * @param {boolean} cond
 */
function expressionVariant(cond) {
  const own = (name) => (cond ? `_cond_${name}` : name);
  /** @param {GrammarSymbols<string>} $ */
  const expr = ($) => (cond ? $._cond_expression : $._expression);
  /** @param {GrammarSymbols<string>} $ @param {string} name */
  const node = ($, name) => (cond ? alias($[`_cond_${name}`], $[name]) : $[name]);

  return {
    [cond ? '_cond_expression' : '_expression']: ($) => choice(
      node($, 'binary'),
      node($, 'unary'),
      node($, 'lazy'),
      node($, 'call'),
      node($, 'field_access'),
      node($, 'index'),
      node($, 'ascription'),
      node($, 'narrowing'),
      node($, 'type_test'),
      node($, 'reference'),
      $._primary,
    ),

    // parser: Expressions, the binding-power table. Comparison and the
    // ranges are non-associative in gcsl; here they are left-associative
    // (a divergence: `a < b < c` parses).
    [own('binary')]: ($) => {
      const table = [
        [PREC.or, 'or'],
        [PREC.and, 'and'],
        [PREC.compare, choice('==', '!=', '===', '!==', '<', '<=', '>', '>=')],
        [PREC.range, choice('..', '..=')],
        [PREC.additive, choice('+', alias($._minus, '-'))],
        [PREC.multiplicative, choice('*', '/', '//', '%')],
      ];
      return choice(...table.map(([precedence, operator]) => prec.left(
        /** @type {number} */ (precedence),
        seq(
          field('left', expr($)),
          field('operator', /** @type {RuleOrLiteral} */ (operator)),
          field('right', expr($)),
        ),
      )));
    },

    // parser: Expressions. `not` takes a comparison; `-` and `await` bind
    // tighter than an ascription but looser than a postfix operator.
    [own('unary')]: ($) => choice(
      prec(PREC.not, seq(field('operator', 'not'), field('operand', expr($)))),
      prec(PREC.unary, seq(field('operator', choice('-', 'await')), field('operand', expr($)))),
    ),

    // parser: Futures. `lazy EXPR` takes the longest expression it can.
    [own('lazy')]: ($) => prec.right(PREC.lazy, seq('lazy', field('body', expr($)))),

    // parser: Functions and calls. A call is a postfix operator: an argument
    // list, then optionally a trailing block, or a trailing block alone. The
    // condition variant has no trailing block (parser: Trailing blocks).
    [own('call')]: ($) => cond
      ? prec(PREC.postfix, seq(field('function', expr($)), field('arguments', $.arguments)))
      : prec.right(PREC.postfix, seq(
        field('function', expr($)),
        choice(
          seq(field('arguments', $.arguments), optional(field('block', $.block))),
          field('block', $.block),
        ),
      )),

    // parser: Expressions, field access `EXPR.NAME`; parser: Names.
    [own('field_access')]: ($) => prec(PREC.postfix, seq(
      field('record', expr($)),
      '.',
      field('field', $._name),
    )),

    // parser: Lists, an index `EXPR[INDEX]`, the position parsed at
    // minimum 0.
    [own('index')]: ($) => prec(PREC.postfix, seq(
      field('list', expr($)),
      '[',
      field('index', $._expression),
      ']',
    )),

    // parser: Expressions, type ascription `EXPR : TYPE`.
    [own('ascription')]: ($) => prec.left(PREC.ascription, seq(
      field('value', expr($)),
      ':',
      field('type', $._type),
    )),

    // parser: Expressions, a checked narrowing `EXPR as TYPE`.
    [own('narrowing')]: ($) => prec.left(PREC.ascription, seq(
      field('value', expr($)),
      'as',
      field('type', $._type),
    )),

    // parser: Expressions, a type test `EXPR is TYPE`, binding as a
    // comparison does.
    [own('type_test')]: ($) => prec.left(PREC.compare, seq(
      field('value', expr($)),
      'is',
      field('type', $._type),
    )),

    // parser: Def-types, a reference `DT PATH`: after a name, or a field
    // access, an identifier starts a definition's path, read whole.
    [own('reference')]: ($) => prec(PREC.postfix, seq(
      field('def_type', choice($._ident, node($, 'field_access'))),
      field('path', $.definition_path),
    )),
  };
}

module.exports = grammar({
  name: 'gcsl',

  word: ($) => $.identifier,

  // lexer: spaces, tabs, `\r` and comments are skipped. A `\n` is skipped
  // too wherever no item can end; where one can, the external scanner makes
  // it a `_newline` (see src/scanner.c).
  extras: ($) => [/\s/, $.comment],

  externals: ($) => [
    $._newline,
    $._string_start,
    $._raw_string_start,
    $.string_content,
    $.escape_sequence,
    $._interpolation_start,
    $._string_end,
    $._error_sentinel,
  ],

  conflicts: ($) => [
    // `defmeta` is contextual (parser: Def-types): `defmeta p q { }` is a
    // meta, but `defmeta p { }` the call of the reference `defmeta p` with a
    // trailing block. Which it is shows only after the name that follows
    // `defmeta`, so both are kept until then.
    [$.meta, $._ident],
    // A field item or a rebind has no keyword (parser: Paths): its path,
    // `a.b[i]` or `a: T`, reads as an expression until an `=` or `:=`
    // shows it to be a path, which gcsl finds by looking ahead
    // (`Parser::starts_path_item`). Both readings are kept until then.
    [$._primary, $._name],
  ],

  rules: {
    // parser: Items and bodies. A file is a body: items separated by
    // newlines or `;`, runs of separators skipped.
    source_file: ($) => seq(
      repeat(';'),
      optional(seq(
        $._top_item,
        repeat(seq($._separator, $._top_item)),
        optional($._separator),
      )),
    ),

    // A separator: a newline or a `;`, and any `;`s after it. Newlines after
    // a separator are skipped as whitespace, since no item can end there.
    _separator: ($) => seq(choice($._newline, ';'), repeat(';')),

    // parser: Items and bodies; parser: Attributes. Outer attributes stand
    // before the item they mark; an inner attribute is an item of its own.
    _top_item: ($) => choice(
      seq(
        repeat($.attribute),
        choice(
          $._item,
          $.import,
          $.type_declaration,
          $.definition,
          $.meta,
        ),
      ),
      $.inner_attribute,
    ),

    // parser: Items and bodies, the items of any body. An expression item
    // is the expression itself, with no node of its own.
    _item: ($) => choice(
      $.let,
      $.field,
      $.rebind,
      $.result,
      $.spread,
      $.named_function,
      $.public_function,
      $.break,
      $.continue,
      $.return,
      $._expression,
    ),

    // parser: Items and bodies, `let PATH (: TYPE)? = EXPR` and `let mut`.
    let: ($) => seq(
      'let',
      optional('mut'),
      field('path', $.path),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._expression),
    ),

    // parser: Paths, a field item `PATH (: TYPE)? = EXPR`.
    field: ($) => seq(
      field('path', $.path),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._expression),
    ),

    // parser: Paths, a rebind `PATH := EXPR`.
    rebind: ($) => seq(
      field('path', $.path),
      ':=',
      field('value', $._expression),
    ),

    // `= EXPR`: the block's result.
    result: ($) => seq('=', field('value', $._expression)),

    // parser: Items and bodies (a spread item) and Lists (a spread
    // element): `...EXPR`, its expression at minimum 0.
    spread: ($) => seq('...', field('value', $._expression)),

    // parser: Items and bodies, a named function `fn NAME(...) ...`.
    named_function: ($) => seq(
      'fn',
      field('name', $._ident),
      $._signature,
      field('body', $.block),
    ),

    // parser: Items and bodies, a public function `def PATH PARAMS RETURN
    // BODY`, named by a path of identifiers.
    public_function: ($) => seq(
      'def',
      field('name', $.dotted_name),
      $._signature,
      field('body', $.block),
    ),

    // parser: Loops, `break`, `continue` and `return`: the keyword, its
    // label, and its value if one begins on the same line.
    break: ($) => jump($, 'break'),
    continue: ($) => jump($, 'continue'),
    return: ($) => jump($, 'return'),

    // parser: Imports.
    //   IMPORT = "import" PATH ("as" NAME)?
    //          | "import" MEMBER ("," MEMBER)* "from" PATH
    // The forms are told apart after the first path or member and its `as`,
    // so both start with `_import_head`, made a member when `,` or `from`
    // follows.
    import: ($) => seq(
      'import',
      choice(
        $._import_head,
        seq(
          choice(alias($._import_head, $.import_member), $.wildcard),
          repeat(seq(',', choice(alias($._import_head, $.import_member), $.wildcard))),
          'from',
          field('source', alias($.dotted_name, $.module_path)),
        ),
      ),
    ),

    _import_head: ($) => seq(
      field('path', alias($.dotted_name, $.module_path)),
      optional(seq('as', field('alias', $._ident))),
    ),

    wildcard: (_) => '*',

    // parser: Types, a `type` item `type NAME = TYPE`. `type` is
    // contextual: a `type` item's start only where an item starts and an
    // identifier follows; anywhere else it is a name (see `_ident`). The
    // precedence makes `type` and an identifier at an item's start this,
    // never a reference whose def-type is a name `type`.
    type_declaration: ($) => prec(PREFERRED, seq(
      'type',
      field('name', $._ident),
      '=',
      field('type', $._type),
    )),

    // parser: Def-types, `DEFINITION = "def" DT PATH "(" NAMES ")" BLOCK`.
    definition: ($) => seq(
      'def',
      field('def_type', alias($.dotted_name, $.def_type)),
      field('path', $.definition_path),
      field('parameters', alias($.definition_parameters, $.parameters)),
      field('body', $.block),
    ),

    // A definition's parameters are written by name alone (parser:
    // Def-types); the tree calls them `parameters` and `parameter`, as a
    // function's are.
    definition_parameters: ($) => seq(
      '(',
      commaSep(alias($._definition_parameter, $.parameter)),
      ')',
    ),

    _definition_parameter: ($) => field('name', $._ident),

    // parser: Def-types, `META = "defmeta" DT PATH (BLOCK | "=" EXPR)`.
    // `defmeta` is contextual, as `type` is.
    meta: ($) => seq(
      'defmeta',
      field('def_type', alias($.dotted_name, $.def_type)),
      field('path', $.definition_path),
      choice(
        field('body', $.block),
        seq('=', field('value', $._expression)),
      ),
    ),

    // Identifiers joined by `.`: a `def`'s name, a def-type (DT), a module
    // path.
    dotted_name: ($) => seq($._ident, repeat(seq('.', $._ident))),

    // parser: Def-types, a definition's PATH: segments joined by `.`, the
    // first starting with an identifier, a later one possibly with a digit,
    // each possibly with `+` glued into it (`gmp-c++`, `go-stage0-1.24`).
    definition_path: ($) => prec.right(seq(
      alias($._first_segment, $.segment),
      repeat(seq('.', $.segment)),
    )),

    _first_segment: ($) => seq($._ident, optional($._glue)),

    segment: ($) => choice(
      seq($._ident, optional($._glue)),
      seq($._digit_segment, optional($._glue)),
    ),

    // The run of letters, digits, `_`, `+` and `-` glued to a segment's
    // identifier, starting with the `+` that directly follows it.
    _glue: (_) => token.immediate(prec(1, /\+([A-Za-z0-9_+]|-[A-Za-z0-9_+])*/)),

    // A segment after a `.` that starts with a digit: `24`, `24rc1`, `1_0`,
    // `1e5` (parser: Def-types).
    _digit_segment: (_) => /[0-9][0-9_]*([eE][+-]?[0-9][0-9_]*)?[A-Za-z0-9_]*/,

    // parser: Paths. `PATH = NAME SEGMENT*`, `SEGMENT = "." NAME | "[" EXPR
    // "]"`; a name is an identifier or a plain string (parser: Names).
    path: ($) => seq(
      field('name', $._name),
      repeat(choice(
        seq('.', field('field', $._name)),
        $.index_segment,
      )),
    ),

    // An index segment, `[EXPR]`, its position any expression. gcsl allows
    // one only in the path of a `:=`.
    index_segment: ($) => seq('[', field('index', $._expression), ']'),

    // ---- Expressions -----------------------------------------------------

    ...expressionVariant(false),
    ...expressionVariant(true),

    // parser: Expressions, the prefix expressions, shared by both variants.
    _primary: ($) => choice(
      $._ident,
      $.integer,
      $.float,
      $.boolean,
      $.string,
      $.raw_string,
      $.parenthesized_expression,
      $.list,
      $.block,
      $.if,
      $.for,
      $.do,
      $.function,
    ),

    parenthesized_expression: ($) => seq('(', $._expression, ')'),

    // parser: Lists, `[` elements `]`, an element an expression or a spread.
    list: ($) => seq('[', commaSep(choice($._expression, $.spread)), ']'),

    // parser: Items and bodies; parser: Labels. A block is a body in braces,
    // optionally labelled directly after its `{`.
    block: ($) => seq(
      '{',
      optional(field('label', $.label)),
      repeat(';'),
      optional(seq(
        $._item,
        repeat(seq($._separator, $._item)),
        optional($._separator),
      )),
      '}',
    ),

    // parser: Expressions, `if`, a condition (with no trailing block), a
    // block, and optionally `else` and a block or another `if`. An `else`
    // may start the next line (parser: Newlines after an `if`): the
    // scanner makes no `_newline` before one.
    if: ($) => prec.right(seq(
      'if',
      field('condition', $._cond_expression),
      field('consequence', $.block),
      optional(seq(
        'else',
        field('alternative', choice($.block, $.if)),
      )),
    )),

    // parser: Loops. After `for` and its label: a name directly followed
    // by `in` begins the iteration; a `{` the body of a loop with no
    // condition; anything else a header of `;`-separated parts.
    // A `{` directly after `for` and its label is always the body, never a
    // block starting a header (parser: Trailing blocks), hence the
    // precedence of the first form.
    for: ($) => choice(
      prec(PREFERRED, seq(
        'for',
        optional(field('label', $.label)),
        field('body', $.block),
      )),
      seq(
        'for',
        optional(field('label', $.label)),
        choice(
          seq(
            field('variable', $._ident),
            'in',
            field('iterable', $._cond_expression),
            optional(seq('if', field('guard', $._cond_expression))),
          ),
          seq($._header_part, repeat(seq(';', $._header_part))),
        ),
        field('body', $.block),
      ),
    ),

    // A part of a `for` header: a `let`, a rebind or the condition, each
    // parsed as an `if`'s condition is.
    _header_part: ($) => choice(
      alias($._cond_let, $.let),
      alias($._cond_rebind, $.rebind),
      field('condition', $._cond_expression),
    ),

    // A header's `let` and rebind: the items, their values in the condition
    // variant (see `expressionVariant`), aliased to the item nodes.
    _cond_let: ($) => seq(
      'let',
      optional('mut'),
      field('path', $.path),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._cond_expression),
    ),

    _cond_rebind: ($) => seq(
      field('path', $.path),
      ':=',
      field('value', $._cond_expression),
    ),

    // parser: Loops, `do BLOCK for COND`, the condition an ordinary
    // expression at minimum 0.
    do: ($) => prec.right(PREC.lazy, seq(
      'do',
      optional(field('label', $.label)),
      field('body', $.block),
      'for',
      field('condition', $._expression),
    )),

    // parser: Functions and calls, `fn(PARAMS) MARKER TYPE BLOCK`.
    function: ($) => seq('fn', $._signature, field('body', $.block)),

    _signature: ($) => seq(
      field('parameters', $.parameters),
      field('effect', $.effect),
      field('return_type', $._type),
    ),

    parameters: ($) => seq('(', commaSep($.parameter), ')'),

    // `NAME: TYPE`, or `NAME: TYPE = DEFAULT`.
    parameter: ($) => seq(
      field('name', $._ident),
      ':',
      field('type', $._type),
      optional(seq('=', field('default', $._expression))),
    ),

    // The effect marker: `:` pure, `->>` impure and memoised, `->` impure.
    effect: (_) => choice(':', '->>', '->'),

    // parser: Functions and calls, an argument list; an argument is an
    // expression or `NAME = EXPR`.
    arguments: ($) => seq(
      '(',
      commaSep(choice($._expression, $.named_argument)),
      ')',
    ),

    named_argument: ($) => seq(
      field('name', $._ident),
      '=',
      field('value', $._expression),
    ),

    // ---- Types (parser: Types) -------------------------------------------

    // A type: a name, a qualified name, an applied type, a record type, a
    // function type, a list type `T[]`, a union or a type in parentheses.
    _type: ($) => choice(
      alias($.identifier, $.type_identifier),
      $.qualified_type,
      $.applied_type,
      $.record_type,
      $.function_type,
      $.list_type,
      $.union_type,
      $.parenthesized_type,
    ),

    // A qualified name, `t.PackageDef`. After an ascription's type, a `.`
    // continues the name: `x : Int.y` is the ascription of `Int.y`.
    qualified_type: ($) => prec(TYPE_PREC.applied, seq(
      $.identifier,
      repeat1(seq('.', $.identifier)),
    )),

    // An applied type, `Future<Int>`: an identifier, `<`, a type, `>`.
    applied_type: ($) => prec(TYPE_PREC.applied, seq(
      field('name', alias($.identifier, $.type_identifier)),
      '<',
      field('argument', $._type),
      '>',
    )),

    // `{` entries `}`, each `NAME: TYPE`, `NAME: TYPE = DEFAULT` or a spread
    // `...TYPE`.
    record_type: ($) => seq(
      '{',
      commaSep(choice($.field_type, alias($.spread_type, $.spread))),
      '}',
    ),

    field_type: ($) => seq(
      field('name', $._name),
      ':',
      field('type', $._type),
      optional(seq('=', field('default', $._expression))),
    ),

    spread_type: ($) => seq('...', field('type', $._type)),

    // `fn(TYPES) MARKER TYPE`, then optionally a meta, `META` MARKER TYPE.
    function_type: ($) => prec.right(TYPE_PREC.function, seq(
      'fn',
      field('parameters', $.parameter_types),
      field('effect', $.effect),
      field('return_type', $._type),
      optional(field('meta', $.meta_type)),
    )),

    parameter_types: ($) => seq('(', commaSep($.parameter_type), ')'),

    // `NAME: TYPE`, recognised by an identifier followed by `:`, or `TYPE`.
    // A parameter type has no default.
    parameter_type: ($) => seq(
      optional(seq(field('name', $._ident), ':')),
      field('type', $._type),
    ),

    meta_type: ($) => seq(
      'META',
      field('effect', $.effect),
      field('type', $._type),
    ),

    // `T[]`: a postfix on any type but a union.
    list_type: ($) => prec(TYPE_PREC.list, seq(
      field('element', $._type),
      alias($._empty_brackets, '[]'),
    )),

    // `[]` as one token, so that after a type a `[` with a `]` directly
    // after it is the list type, and any other `[` an index.
    _empty_brackets: (_) => token(seq('[', /\s*/, ']')),

    union_type: ($) => prec.left(TYPE_PREC.union, seq(
      field('left', $._type),
      '|',
      field('right', $._type),
    )),

    parenthesized_type: ($) => seq('(', $._type, ')'),

    // ---- Names and literals ----------------------------------------------

    // parser: Names. A field's name is an identifier or a plain string.
    _name: ($) => choice($._ident, $.string),

    // An identifier, or the contextual keywords `type` and `defmeta` where
    // they are names (parser: Types, Def-types).
    _ident: ($) => choice(
      $.identifier,
      alias(choice('type', 'defmeta'), $.identifier),
    ),

    identifier: (_) => new RegExp(IDENT),

    // lexer: a label is `'` directly followed by a name.
    label: (_) => token.immediate(new RegExp(`'${IDENT}`)),

    // lexer: number literals. Digits with single `_` separators; a float has
    // a fraction or an exponent.
    integer: (_) => /[0-9](_?[0-9])*/,

    float: (_) => /[0-9](_?[0-9])*(\.[0-9](_?[0-9])*([eE][+-]?[0-9](_?[0-9])*)?|[eE][+-]?[0-9](_?[0-9])*)/,

    boolean: (_) => choice('true', 'false'),

    // lexer: String literals. The scanner lexes the delimiters, the text,
    // the escapes and an interpolation's `{`.
    string: ($) => seq(
      $._string_start,
      repeat(choice($.string_content, $.escape_sequence, $.interpolation)),
      $._string_end,
    ),

    // `r` directly followed by the opening quotes: every byte is text.
    raw_string: ($) => seq(
      $._raw_string_start,
      optional($.string_content),
      $._string_end,
    ),

    // A single `{` in a literal's text, an expression, and the `}` that
    // matches it (lexer: Contexts). Newlines are skipped directly inside it.
    interpolation: ($) => seq(
      alias($._interpolation_start, '{'),
      field('expression', $._expression),
      '}',
    ),

    // Subtraction's `-` is followed by whitespace; negation's directly by
    // its operand (lexer, `-`).
    _minus: (_) => token(/-\s/),

    // lexer: Attributes. `#[NAME]` and `#![NAME]`; any other `#` starts a
    // comment.
    attribute: (_) => token(prec(1, new RegExp(`#\\[[ \\t]*${IDENT}[ \\t]*\\]`))),

    inner_attribute: (_) => token(prec(1, new RegExp(`#!\\[[ \\t]*${IDENT}[ \\t]*\\]`))),

    comment: (_) => token(prec(-1, /#[^\n]*/)),
  },
});
