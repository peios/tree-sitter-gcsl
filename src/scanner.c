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
 * Plain C11: creation allocates one fixed state; scanning does not allocate.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

enum StringMode { INTERPOLATED_STRING, RAW_STRING };

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

/* Keep incremental scanner snapshots bounded. Past this depth, the editor
 * parser opens no more literals; this is not the GCSL runtime's depth limit. */
#define MAX_STRINGS 64

enum { QUOTE_BYTES = 4, FRAME_BYTES = QUOTE_BYTES + 1 };

_Static_assert(1 + FRAME_BYTES * MAX_STRINGS <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE,
               "scanner state must fit Tree-sitter snapshots");

struct Scanner {
    /* Per open literal, innermost last: its full quote count and raw mode. */
    uint32_t quotes[MAX_STRINGS];
    uint8_t raw[MAX_STRINGS];
    uint8_t depth;
};

static inline void advance(TSLexer *lexer) {
    lexer->advance(lexer, false);
}

static inline void skip(TSLexer *lexer) {
    lexer->advance(lexer, true);
}

static inline bool is_ident_byte(int32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* After the `\n` of a possible `_newline`: whether the next word, past
 * whitespace, blank lines and comment lines, is `else`. Only looks ahead;
 * the token's end is already marked. */
static bool else_follows(TSLexer *lexer) {
    for (;;) {
        int32_t c = lexer->lookahead;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            advance(lexer);
        } else if (c == '#') {
            while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
                advance(lexer);
            }
        } else {
            break;
        }
    }
    for (const char *letter = "else"; *letter; letter++) {
        if (lexer->lookahead != *letter) {
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

/* Scan an escape beginning at a backslash; semantic escape validation belongs to GCSL. */
static bool scan_escape(TSLexer *lexer, const bool *valid_symbols) {
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

/* A piece of the innermost open literal: its text, an escape, an
 * interpolation's `{`, or its closing quotes. */
static bool scan_piece(struct Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
    uint32_t quotes = scanner->quotes[scanner->depth - 1];
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
            uint32_t run = 0;
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
            if (c == '\\') return scan_escape(lexer, valid_symbols);
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
static bool scan_start(struct Scanner *scanner, TSLexer *lexer, enum StringMode mode) {
    bool raw = mode == RAW_STRING;
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
    uint32_t run = 1;
    while (lexer->lookahead == '"') {
        if (run == UINT32_MAX) return false;
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
    scanner->quotes[scanner->depth] = run;
    scanner->raw[scanner->depth] = raw;
    scanner->depth++;
    lexer->result_symbol = raw ? RAW_STRING_START : STRING_START;
    return true;
}

void *tree_sitter_gcsl_external_scanner_create(void) {
    return ts_calloc(1, sizeof(struct Scanner));
}

void tree_sitter_gcsl_external_scanner_destroy(void *payload) {
    ts_free(payload);
}

unsigned tree_sitter_gcsl_external_scanner_serialize(void *payload, char *buffer) {
    struct Scanner *scanner = (struct Scanner *)payload;
    unsigned size = 0;
    buffer[size++] = (char)scanner->depth;
    /* Explicit little-endian counts keep snapshots independent of struct layout. */
    for (unsigned i = 0; i < scanner->depth; i++) {
        uint32_t quotes = scanner->quotes[i];
        for (unsigned byte = 0; byte < QUOTE_BYTES; byte++) {
            buffer[size++] = (char)(quotes & UINT8_MAX);
            quotes >>= 8;
        }
        buffer[size++] = (char)scanner->raw[i];
    }
    return size;
}

void tree_sitter_gcsl_external_scanner_deserialize(void *payload, const char *buffer,
                                                   unsigned length) {
    struct Scanner *scanner = (struct Scanner *)payload;
    memset(scanner, 0, sizeof(struct Scanner));
    if (length == 0) {
        return;
    }
    uint8_t depth = (uint8_t)buffer[0];
    if (depth > MAX_STRINGS || length != 1u + FRAME_BYTES * depth) return;
    struct Scanner restored = {0};
    unsigned offset = 1;
    for (unsigned i = 0; i < depth; i++) {
        uint32_t quotes = 0;
        for (unsigned byte = 0; byte < QUOTE_BYTES; byte++) {
            quotes |= (uint32_t)(uint8_t)buffer[offset++] << (8 * byte);
        }
        uint8_t raw = (uint8_t)buffer[offset++];
        if (quotes == 0 || quotes == 2 || raw > 1) return;
        restored.quotes[i] = quotes;
        restored.raw[i] = raw;
    }
    restored.depth = depth;
    *scanner = restored;
}

bool tree_sitter_gcsl_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
    struct Scanner *scanner = (struct Scanner *)payload;
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
        return scan_start(scanner, lexer, INTERPOLATED_STRING);
    }
    if (lexer->lookahead == 'r' && valid_symbols[RAW_STRING_START]) {
        return scan_start(scanner, lexer, RAW_STRING);
    }
    return false;
}
