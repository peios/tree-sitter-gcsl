#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tree_sitter/api.h>

const TSLanguage *tree_sitter_gcsl(void);

static void check(bool condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static TSTree *parse(TSParser *parser, const char *source, const TSTree *old) {
    TSTree *tree = ts_parser_parse_string(parser, old, source, (uint32_t)strlen(source));
    check(tree != NULL, "parse returned no tree");
    check(!ts_node_has_error(ts_tree_root_node(tree)), source);
    return tree;
}

static void compare_edit(TSParser *parser, const char *before, const char *after,
                         TSInputEdit edit) {
    TSTree *old = parse(parser, before, NULL);
    ts_tree_edit(old, &edit);
    TSTree *incremental = parse(parser, after, old);
    TSTree *fresh = parse(parser, after, NULL);
    char *left = ts_node_string(ts_tree_root_node(incremental));
    char *right = ts_node_string(ts_tree_root_node(fresh));
    check(left != NULL && right != NULL, "tree string allocation failed");
    check(strcmp(left, right) == 0, "incremental and fresh trees differ");
    check(ts_node_end_byte(ts_tree_root_node(incremental)) == strlen(after),
          "incremental tree length differs");
    free(left);
    free(right);
    ts_tree_delete(old);
    ts_tree_delete(incremental);
    ts_tree_delete(fresh);
}

static char *read_query(const char *name) {
    FILE *file = fopen(name, "rb");
    check(file != NULL, "query could not be opened");
    check(fseek(file, 0, SEEK_END) == 0, "query seek failed");
    long size = ftell(file);
    check(size >= 0 && size < 1000000, "query size invalid");
    rewind(file);
    char *text = calloc((size_t)size + 1, 1);
    check(text != NULL, "query allocation failed");
    check(fread(text, 1, (size_t)size, file) == (size_t)size, "query read failed");
    check(fclose(file) == 0, "query close failed");
    return text;
}

static bool contains_capture(TSQuery *query, TSTree *tree, const char *wanted) {
    TSQueryCursor *cursor = ts_query_cursor_new();
    check(cursor != NULL, "query cursor allocation failed");
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
    TSQueryMatch match;
    bool found = false;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t i = 0; i < match.capture_count; i++) {
            uint32_t length;
            const char *name =
                ts_query_capture_name_for_id(query, match.captures[i].index, &length);
            if (strlen(wanted) == length && memcmp(name, wanted, length) == 0) found = true;
        }
    }
    ts_query_cursor_delete(cursor);
    return found;
}

static void test_queries(TSParser *parser) {
    const char *source = "fn greet(name: String): String {\n let result = name\n = result\n}\n";
    TSTree *tree = parse(parser, source, NULL);
    const char *paths[] = {"queries/highlights.scm", "queries/indents.scm", "queries/folds.scm",
                           "queries/textobjects.scm"};
    const char *captures[] = {"function", "indent.begin", "fold", "function.outer"};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
        char *text = read_query(paths[i]);
        uint32_t offset;
        TSQueryError error;
        TSQuery *query =
            ts_query_new(tree_sitter_gcsl(), text, (uint32_t)strlen(text), &offset, &error);
        check(query != NULL, paths[i]);
        check(contains_capture(query, tree, captures[i]), "expected query capture missing");
        ts_query_delete(query);
        free(text);
    }
    ts_tree_delete(tree);
}

static char *long_string(unsigned quotes, bool raw, const char *body) {
    const char *prefix = raw ? "let text = r" : "let text = ";
    size_t size = strlen(prefix) + 2 * quotes + strlen(body) + 2;
    char *source = calloc(size, 1);
    check(source != NULL, "long string allocation failed");
    char *position = source;
    memcpy(position, prefix, strlen(prefix));
    position += strlen(prefix);
    memset(position, '"', quotes);
    position += quotes;
    memcpy(position, body, strlen(body));
    position += strlen(body);
    memset(position, '"', quotes);
    position[quotes] = '\n';
    return source;
}

static void test_long_strings(TSParser *parser) {
    const unsigned lengths[] = {255, 256, 1024};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(*lengths); i++) {
        for (unsigned raw = 0; raw < 2; raw++) {
            char *before = long_string(lengths[i], raw, "abc");
            char *after = long_string(lengths[i], raw, "a{\"nested\"}bc");
            uint32_t offset = (uint32_t)strlen("let text = ") + raw + lengths[i] + 1;
            uint32_t inserted = (uint32_t)strlen("{\"nested\"}");
            compare_edit(parser, before, after,
                         (TSInputEdit){
                             .start_byte = offset,
                             .old_end_byte = offset,
                             .new_end_byte = offset + inserted,
                             .start_point = {0, offset},
                             .old_end_point = {0, offset},
                             .new_end_point = {0, offset + inserted},
                         });
            free(before);
            free(after);
        }
    }
}

int main(void) {
    TSParser *parser = ts_parser_new();
    check(parser != NULL, "parser allocation failed");
    check(ts_parser_set_language(parser, tree_sitter_gcsl()), "unsupported parser ABI");
    const char *samples[] = {
        "let text = \"hello {\"nested\"}\"\n",
        "let text = r\"\"\"{raw}\\n\"\"\"\n",
        "if true { 1 }\n# between branches\nelse { 2 }\n",
        "let elsewhere = 2\nif true { 1 }\nelsewhere\n",
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(*samples); i++) {
        TSTree *tree = parse(parser, samples[i], NULL);
        ts_tree_delete(tree);
    }
    compare_edit(parser, "let x = \"abc\"\n", "let x = \"a{1}bc\"\n",
                 (TSInputEdit){
                     .start_byte = 10,
                     .old_end_byte = 10,
                     .new_end_byte = 13,
                     .start_point = {0, 10},
                     .old_end_point = {0, 10},
                     .new_end_point = {0, 13},
                 });
    test_queries(parser);
    test_long_strings(parser);
    ts_parser_delete(parser);
    return 0;
}
