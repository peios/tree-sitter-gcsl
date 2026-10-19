#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree_sitter/parser.h"

void *tree_sitter_gcsl_external_scanner_create(void);
void tree_sitter_gcsl_external_scanner_destroy(void *payload);
unsigned tree_sitter_gcsl_external_scanner_serialize(void *payload, char *buffer);
void tree_sitter_gcsl_external_scanner_deserialize(void *payload, const char *buffer,
                                                   unsigned length);
bool tree_sitter_gcsl_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid);

enum Token { NEWLINE, STRING_START, RAW_STRING_START, CONTENT, ESCAPE, INTERPOLATION, END, ERROR };

struct Input {
    TSLexer lexer;
    const char *text;
    size_t offset;
    size_t end;
};

static void check(bool condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void advance(TSLexer *lexer, bool skip) {
    (void)skip;
    struct Input *input = (struct Input *)lexer;
    if (input->text[input->offset]) input->offset++;
    lexer->lookahead = (unsigned char)input->text[input->offset];
}

static void mark_end(TSLexer *lexer) {
    struct Input *input = (struct Input *)lexer;
    input->end = input->offset;
}

static bool eof(const TSLexer *lexer) {
    const struct Input *input = (const struct Input *)lexer;
    return !input->text[input->offset];
}

static struct Input input(const char *text) {
    return (struct Input){
        .lexer = {.lookahead = (unsigned char)text[0],
                  .advance = advance,
                  .mark_end = mark_end,
                  .eof = eof},
        .text = text,
    };
}

static void test_round_trip(void) {
    void *scanner = tree_sitter_gcsl_external_scanner_create();
    check(scanner != NULL, "scanner allocation failed");
    bool valid[8] = {[RAW_STRING_START] = true};
    struct Input opening = input("r\"\"\"");
    check(tree_sitter_gcsl_external_scanner_scan(scanner, &opening.lexer, valid),
          "raw start failed");
    char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned size = tree_sitter_gcsl_external_scanner_serialize(scanner, serialized);
    void *restored = tree_sitter_gcsl_external_scanner_create();
    check(restored != NULL, "restored scanner allocation failed");
    tree_sitter_gcsl_external_scanner_deserialize(restored, serialized, size);
    char copy[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    check(tree_sitter_gcsl_external_scanner_serialize(restored, copy) == size,
          "state size changed");
    check(memcmp(serialized, copy, size) == 0, "state bytes changed");
    bool pieces[8] = {[CONTENT] = true, [END] = true};
    struct Input body = input("{literal}\\n\"\"\"");
    check(tree_sitter_gcsl_external_scanner_scan(restored, &body.lexer, pieces),
          "raw content failed");
    check(body.lexer.result_symbol == CONTENT && body.end == 11,
          "raw content interpreted an escape");
    tree_sitter_gcsl_external_scanner_destroy(restored);
    tree_sitter_gcsl_external_scanner_destroy(scanner);
}

static void test_depth_and_reset(void) {
    void *scanner = tree_sitter_gcsl_external_scanner_create();
    check(scanner != NULL, "scanner allocation failed");
    bool valid[8] = {[STRING_START] = true};
    for (unsigned i = 0; i < 64; i++) {
        struct Input opening = input("\"");
        check(tree_sitter_gcsl_external_scanner_scan(scanner, &opening.lexer, valid),
              "nesting rejected too soon");
    }
    struct Input too_deep = input("\"");
    check(!tree_sitter_gcsl_external_scanner_scan(scanner, &too_deep.lexer, valid),
          "nesting cap exceeded");
    char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned size = tree_sitter_gcsl_external_scanner_serialize(scanner, serialized);
    check(size <= sizeof(serialized), "serialized state exceeds Tree-sitter's buffer");
    tree_sitter_gcsl_external_scanner_deserialize(scanner, serialized, size - 1);
    struct Input opening = input("\"");
    check(tree_sitter_gcsl_external_scanner_scan(scanner, &opening.lexer, valid),
          "truncated state was not reset");
    tree_sitter_gcsl_external_scanner_deserialize(scanner, NULL, 0);
    check(tree_sitter_gcsl_external_scanner_serialize(scanner, serialized) == 1,
          "empty state was not reset");
    tree_sitter_gcsl_external_scanner_destroy(scanner);
}

static void test_newline(void) {
    void *scanner = tree_sitter_gcsl_external_scanner_create();
    check(scanner != NULL, "scanner allocation failed");
    bool valid[8] = {[NEWLINE] = true};
    const char *continuations[] = {"\nelse {", "\n # comment\n\n else if"};
    for (unsigned i = 0; i < sizeof(continuations) / sizeof(*continuations); i++) {
        struct Input next = input(continuations[i]);
        check(!tree_sitter_gcsl_external_scanner_scan(scanner, &next.lexer, valid),
              "else was split from if");
    }
    const char *items[] = {"\nelsewhere", "\nelse-name", "\nother", "\n"};
    for (unsigned i = 0; i < sizeof(items) / sizeof(*items); i++) {
        struct Input next = input(items[i]);
        check(tree_sitter_gcsl_external_scanner_scan(scanner, &next.lexer, valid),
              "item separator lost");
        check(next.end == 1 && next.lexer.result_symbol == NEWLINE,
              "newline consumed following item");
    }
    tree_sitter_gcsl_external_scanner_destroy(scanner);
}

static void test_long_delimiter_stack(void) {
    void *scanner = tree_sitter_gcsl_external_scanner_create();
    check(scanner != NULL, "scanner allocation failed");
    char delimiter[400];
    for (unsigned i = 0; i < 64; i++) {
        unsigned raw = i % 2;
        unsigned quotes = 255 + i;
        delimiter[0] = 'r';
        memset(delimiter + raw, '"', quotes);
        delimiter[raw + quotes] = '\0';
        bool valid[8] = {[STRING_START] = !raw, [RAW_STRING_START] = raw};
        struct Input opening = input(delimiter);
        check(tree_sitter_gcsl_external_scanner_scan(scanner, &opening.lexer, valid),
              "long delimiter start failed");
        check(opening.end == raw + quotes, "opening delimiter length changed");
    }
    char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned size = tree_sitter_gcsl_external_scanner_serialize(scanner, serialized);
    check(size <= sizeof(serialized), "long delimiter stack exceeds serialization buffer");
    tree_sitter_gcsl_external_scanner_destroy(scanner);
    scanner = tree_sitter_gcsl_external_scanner_create();
    check(scanner != NULL, "restored scanner allocation failed");
    tree_sitter_gcsl_external_scanner_deserialize(scanner, serialized, size);
    bool pieces[8] = {[CONTENT] = true, [END] = true};
    for (unsigned depth = 64; depth > 0; depth--) {
        unsigned quotes = 254 + depth;
        memset(delimiter, '"', quotes);
        delimiter[quotes] = '\0';
        struct Input closing = input(delimiter);
        check(tree_sitter_gcsl_external_scanner_scan(scanner, &closing.lexer, pieces),
              "restored long delimiter did not close");
        check(closing.lexer.result_symbol == END && closing.end == quotes,
              "restored closing delimiter length changed");
    }
    check(tree_sitter_gcsl_external_scanner_serialize(scanner, serialized) == 1,
          "long delimiter stack did not empty");
    tree_sitter_gcsl_external_scanner_destroy(scanner);
}

int main(void) {
    test_round_trip();
    test_long_delimiter_stack();
    test_depth_and_reset();
    test_newline();
    return 0;
}
