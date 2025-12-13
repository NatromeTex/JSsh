// highlight.h - Syntax highlighting functions
#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "buffer.h"
#include "semantic.h"

// Syntax color pairs
#define SY_KEYWORD  10
#define SY_TYPE     11
#define SY_FUNCTION 12
#define SY_STRING   13
#define SY_NUMBER   14
#define SY_COMMENT  15

// Clear all semantic tokens from buffer
void semantic_tokens_clear(Buffer *buf);

// Push a semantic token to buffer
void semantic_token_push(Buffer *buf, const SemanticToken *tok);

// Map semantic kind to ncurses color pair
int color_for_semantic_kind(SemanticKind kind);

// Get SemanticKind at given line/column
SemanticKind semantic_kind_at(Buffer *buf, int line, int col);

#endif
