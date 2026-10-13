/*
 * The external scanner for tree-sitter-gcsl.
 *
 * It lexes two things a tree-sitter lexer cannot:
 *
 * - The newline that ends an item (gcsl lexer: the newline rule; parser:
 *   Newlines after an `if`). `_newline` is valid only where an item can end,
 *   so a `\n` anywhere else (inside `(` or `[`, after an operator) is
 *   skipped as whitespace. Where it is valid, a `\n` is one unless the next
 *   word, past blank lines and comments, is `else`, which continues an `if`.
 *
 * - String literals (gcsl lexer: String literals). A literal is delimited by
 *   n quotes on each side, n being 1 or at least 3; inside, a run of fewer
 *   than n quotes is text, and the first n quotes of a longer run close it.
 *   `""` is the empty string. With the `r` prefix every byte is text;
 *   otherwise `\` begins an escape, `{{` and `}}` are literal braces, and a
 *   single `{` begins an interpolation, whose expression the grammar parses.
 *   Literals nest through interpolations, so the scanner keeps a stack of the
 *   open ones' delimiters, of fixed size.
 *
 * Plain C99, no allocation: the state is a fixed array, serialized whole.
 */

#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum TokenType {
  NEWLINE,
  STRING_START,
  RAW_STRING_START,
  STRING_CONTENT,
  ESCAPE_SEQUENCE,
  INTERPOLATION_START,
  STRING_END,
  ERROR_SENTINEL,
};

/* How deeply literals may nest in each other's interpolations. gcsl itself
 * stops far sooner (MAX_DEPTH); past this the scanner opens no more. */
#define MAX_STRINGS 64

typedef struct {
  /* Per open literal, innermost last: its quote count (capped at 255) and
   * whether it is raw. */
  uint8_t quotes[MAX_STRINGS];
  uint8_t raw[MAX_STRINGS];
  uint8_t depth;
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static inline bool is_ident_byte(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

/* After the `\n` of a possible `_newline`: whether the next word, past
 * whitespace, blank lines and comment lines, is `else`. Only looks ahead;
 * the token's end is already marked. */
static bool else_follows(TSLexer *lexer) {
  for (;;) {
    int32_t c = lexer->lookahead;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
        c == '\v') {
      advance(lexer);
    } else if (c == '#') {
      while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
        advance(lexer);
      }
    } else {
      break;
    }
  }
  const char *word = "else";
  for (int i = 0; i < 4; i++) {
    if (lexer->lookahead != word[i]) {
      return false;
    }
    advance(lexer);
  }
  /* `else` must be the whole identifier: not `elsewhere`, nor `else-x`. */
  if (is_ident_byte(lexer->lookahead)) {
    return false;
  }
  if (lexer->lookahead == '-') {
    advance(lexer);
    return !is_ident_byte(lexer->lookahead);
  }
  return true;
}

/* A piece of the innermost open literal: its text, an escape, an
 * interpolation's `{`, or its closing quotes. */
static bool scan_piece(Scanner *scanner, TSLexer *lexer,
                       const bool *valid_symbols) {
  unsigned quotes = scanner->quotes[scanner->depth - 1];
  bool raw = scanner->raw[scanner->depth - 1];
  bool has_text = false;

  for (;;) {
    int32_t c = lexer->lookahead;

    if (lexer->eof(lexer)) {
      /* Unterminated: end the text here and let the parser report it. */
      if (has_text) {
        lexer->mark_end(lexer);
        lexer->result_symbol = STRING_CONTENT;
        return true;
      }
      return false;
    }

    if (c == '"') {
      /* The text, if any, ends before the run; count the run up to n. */
      lexer->mark_end(lexer);
      unsigned run = 0;
      while (lexer->lookahead == '"' && run < quotes) {
        advance(lexer);
        run++;
      }
      if (run >= quotes) {
        if (has_text) {
          lexer->result_symbol = STRING_CONTENT;
          return true;
        }
        if (!valid_symbols[STRING_END]) {
          return false;
        }
        lexer->mark_end(lexer);
        scanner->depth--;
        lexer->result_symbol = STRING_END;
        return true;
      }
      /* Fewer than n quotes: text. */
      has_text = true;
      continue;
    }

    if (!raw && (c == '\\' || c == '{' || c == '}')) {
      if (has_text) {
        lexer->mark_end(lexer);
        lexer->result_symbol = STRING_CONTENT;
        return true;
      }
      if (c == '\\') {
        /* An escape: `\` and the character after it, a line break
         * included, or `\u{...}` whole. Which escapes are known is gcsl's
         * to say. */
        advance(lexer);
        if (lexer->lookahead == 'u') {
          advance(lexer);
          if (lexer->lookahead == '{') {
            advance(lexer);
            while (is_ident_byte(lexer->lookahead)) {
              advance(lexer);
            }
            if (lexer->lookahead == '}') {
              advance(lexer);
            }
          }
        } else if (!lexer->eof(lexer)) {
          advance(lexer);
        }
        lexer->mark_end(lexer);
        lexer->result_symbol = ESCAPE_SEQUENCE;
        return valid_symbols[ESCAPE_SEQUENCE];
      }
      if (c == '{') {
        advance(lexer);
        lexer->mark_end(lexer);
        if (lexer->lookahead == '{') {
          advance(lexer);
          lexer->mark_end(lexer);
          lexer->result_symbol = ESCAPE_SEQUENCE;
          return valid_symbols[ESCAPE_SEQUENCE];
        }
        lexer->result_symbol = INTERPOLATION_START;
        return valid_symbols[INTERPOLATION_START];
      }
      /* `}`: `}}` is a literal brace; gcsl rejects a lone one, which is
       * read here as text. */
      advance(lexer);
      if (lexer->lookahead == '}') {
        advance(lexer);
        lexer->mark_end(lexer);
        lexer->result_symbol = ESCAPE_SEQUENCE;
        return valid_symbols[ESCAPE_SEQUENCE];
      }
      has_text = true;
      continue;
    }

    advance(lexer);
    has_text = true;
  }
}

/* The opening delimiter of a literal, at `"` or (raw) at `r"`. */
static bool scan_start(Scanner *scanner, TSLexer *lexer, bool raw) {
  if (raw) {
    advance(lexer); /* the `r` */
    if (lexer->lookahead != '"') {
      return false;
    }
  }
  if (scanner->depth >= MAX_STRINGS) {
    return false;
  }
  advance(lexer);
  lexer->mark_end(lexer);
  unsigned run = 1;
  while (lexer->lookahead == '"') {
    advance(lexer);
    run++;
  }
  if (run == 2) {
    /* `""`: the empty string. The token is its first quote, n is 1, and
     * the second quote closes it. */
    run = 1;
  } else {
    lexer->mark_end(lexer);
  }
  scanner->quotes[scanner->depth] = run > 255 ? 255 : (uint8_t)run;
  scanner->raw[scanner->depth] = raw;
  scanner->depth++;
  lexer->result_symbol = raw ? RAW_STRING_START : STRING_START;
  return true;
}

void *tree_sitter_gcsl_external_scanner_create(void) {
  return ts_calloc(1, sizeof(Scanner));
}

void tree_sitter_gcsl_external_scanner_destroy(void *payload) {
  ts_free(payload);
}

unsigned tree_sitter_gcsl_external_scanner_serialize(void *payload,
                                                     char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  unsigned size = 0;
  buffer[size++] = (char)scanner->depth;
  memcpy(&buffer[size], scanner->quotes, scanner->depth);
  size += scanner->depth;
  memcpy(&buffer[size], scanner->raw, scanner->depth);
  size += scanner->depth;
  return size;
}

void tree_sitter_gcsl_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  memset(scanner, 0, sizeof(Scanner));
  if (length == 0) {
    return;
  }
  uint8_t depth = (uint8_t)buffer[0];
  if (depth > MAX_STRINGS || length < 1u + 2u * depth) {
    return;
  }
  scanner->depth = depth;
  memcpy(scanner->quotes, &buffer[1], depth);
  memcpy(scanner->raw, &buffer[1 + depth], depth);
}

bool tree_sitter_gcsl_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  Scanner *scanner = (Scanner *)payload;
  /* In error recovery every symbol is valid; lex no string pieces then. */
  bool recovering = valid_symbols[ERROR_SENTINEL];

  if (!recovering && scanner->depth > 0 &&
      (valid_symbols[STRING_CONTENT] || valid_symbols[STRING_END])) {
    return scan_piece(scanner, lexer, valid_symbols);
  }

  for (;;) {
    int32_t c = lexer->lookahead;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
      skip(lexer);
    } else if (c == '\n') {
      if (valid_symbols[NEWLINE]) {
        advance(lexer);
        lexer->mark_end(lexer);
        lexer->result_symbol = NEWLINE;
        return !else_follows(lexer);
      }
      skip(lexer);
    } else {
      break;
    }
  }

  if (lexer->lookahead == '"' && valid_symbols[STRING_START]) {
    return scan_start(scanner, lexer, false);
  }
  if (lexer->lookahead == 'r' && valid_symbols[RAW_STRING_START]) {
    return scan_start(scanner, lexer, true);
  }
  return false;
}
