// highlight.h - Syntax highlighting functions
#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "buffer.h"
#include "semantic.h"
#include "language.h"
#include <regex.h>

// Syntax color pairs
#define SY_KEYWORD  10
#define SY_TYPE     11
#define SY_FUNCTION 12
#define SY_STRING   13
#define SY_NUMBER   14
#define SY_COMMENT  15

// Highlight rule flags
#define HL_FLAG_NONE        0
#define HL_FLAG_MULTILINE   (1 << 0)  // For block comments

// A single highlight rule (regex pattern -> SemanticKind)
typedef struct {
    const char *pattern;    // Regex pattern string
    SemanticKind kind;      // Token type to assign
    int flags;              // HL_FLAG_* flags
    regex_t compiled;       // Compiled regex (filled at runtime)
    int is_compiled;        // Whether regex has been compiled
} HighlightRule;

// Language-specific highlighter definition
typedef struct {
    FileType ft;                    // Which file type this applies to
    HighlightRule *rules;           // Array of rules
    size_t rule_count;              // Number of rules
    const char *block_comment_start; // e.g., "/*"
    const char *block_comment_end;   // e.g., "*/"
} LanguageHighlighter;

// Get highlighter for a file type (returns NULL if none)
LanguageHighlighter *get_highlighter(FileType ft);

// Perform regex-based syntax highlighting on a buffer
void highlight_buffer(Buffer *buf);

// Cleanup compiled regexes (call on exit)
void highlight_cleanup(void);

// Clear all semantic tokens from buffer
void semantic_tokens_clear(Buffer *buf);

// Clear only regex-based tokens (keeps LSP tokens)
void semantic_tokens_clear_regex(Buffer *buf);

// Clear only LSP-based tokens (keeps regex tokens)
void semantic_tokens_clear_lsp(Buffer *buf);

// Push a semantic token to buffer
void semantic_token_push(Buffer *buf, const SemanticToken *tok);

// Map semantic kind to ncurses color pair
int color_for_semantic_kind(SemanticKind kind);

// Get SemanticKind at given line/column (prefers LSP over regex)
SemanticKind semantic_kind_at(Buffer *buf, int line, int col);

#endif