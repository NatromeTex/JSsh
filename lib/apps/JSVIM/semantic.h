//lib/apps/JSVIM/semantic.h 
#ifndef SEMANTIC_H
#define SEMANTIC_H

typedef enum {
    SEM_NONE = 0,

    SEM_KEYWORD,
    SEM_TYPE,
    SEM_FUNCTION,
    SEM_VARIABLE,
    SEM_PARAMETER,
    SEM_PROPERTY,
    SEM_MACRO,

    SEM_STRING,
    SEM_NUMBER,
    SEM_COMMENT,

    SEM_NAMESPACE,
    SEM_CLASS,
    SEM_STRUCT,
    SEM_ENUM,

    SEM_OPERATOR,

    SEM_MAX
} SemanticKind;

// Token source: regex (basic) or LSP (semantic)
typedef enum {
    TOKEN_SOURCE_REGEX = 0,
    TOKEN_SOURCE_LSP = 1,
} TokenSource;

typedef struct {
    int line;
    int col;
    int len;
    SemanticKind kind;
    int modifiers;
    TokenSource source;  // Where this token came from
} SemanticToken;

SemanticKind semantic_kind_from_lsp(const char *name);

#endif
