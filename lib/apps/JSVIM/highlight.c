// highlight.c - Syntax highlighting functions
#include "highlight.h"
#include <stdlib.h>

void semantic_tokens_clear(Buffer *buf) {
    if (!buf) return;
    buf->token_count = 0;
}

void semantic_token_push(Buffer *buf, const SemanticToken *tok) {
    if (!buf || !tok) return;

    if (buf->token_count == buf->token_cap) {
        size_t new_cap = buf->token_cap ? buf->token_cap * 2 : 64;
        SemanticToken *new_tokens =
            realloc(buf->tokens, new_cap * sizeof(SemanticToken));
        if (!new_tokens) return;

        buf->tokens = new_tokens;
        buf->token_cap = new_cap;
    }

    buf->tokens[buf->token_count++] = *tok;
}

int color_for_semantic_kind(SemanticKind kind) {
    switch (kind) {
        case SEM_KEYWORD:   return SY_KEYWORD;
        case SEM_TYPE:      return SY_TYPE;
        case SEM_CLASS:     return SY_TYPE;
        case SEM_STRUCT:    return SY_TYPE;
        case SEM_ENUM:      return SY_TYPE;
        case SEM_NAMESPACE: return SY_TYPE;
        case SEM_FUNCTION:  return SY_FUNCTION;
        case SEM_VARIABLE:  return SY_KEYWORD;   // Use keyword color for variables
        case SEM_PARAMETER: return SY_KEYWORD;   // Use keyword color for parameters
        case SEM_PROPERTY:  return SY_FUNCTION;  // Use function color for properties
        case SEM_MACRO:     return SY_FUNCTION;  // Macros in function color
        case SEM_STRING:    return SY_STRING;
        case SEM_NUMBER:    return SY_NUMBER;
        case SEM_COMMENT:   return SY_COMMENT;
        case SEM_OPERATOR:  return 0;            // No special color for operators
        default:            return 0;
    }
}

SemanticKind semantic_kind_at(Buffer *buf, int line, int col) {
    for (size_t i = 0; i < buf->token_count; i++) {
        SemanticToken *t = &buf->tokens[i];
        if (t->line != line)
            continue;

        if (col >= t->col && col < t->col + t->len)
            return t->kind;
    }
    return SEM_NONE;
}
