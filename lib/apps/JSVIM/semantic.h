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

typedef struct {
    int line;
    int col;
    int len;
    SemanticKind kind;
    int modifiers;
} SemanticToken;

SemanticKind semantic_kind_from_lsp(const char *name);

#endif
