/* semantic_lsp.c */

#include "semantic.h"
#include <stdlib.h>
#include <string.h>

SemanticKind semantic_kind_from_lsp(const char *name) {
    if (!name) return SEM_NONE;

    if (strcmp(name, "keyword") == 0) return SEM_KEYWORD;
    if (strcmp(name, "modifier") == 0) return SEM_KEYWORD;  // extern, static, const, etc.
    if (strcmp(name, "type") == 0) return SEM_TYPE;
    if (strcmp(name, "class") == 0) return SEM_CLASS;
    if (strcmp(name, "struct") == 0) return SEM_STRUCT;
    if (strcmp(name, "enum") == 0) return SEM_ENUM;
    if (strcmp(name, "interface") == 0) return SEM_TYPE;
    if (strcmp(name, "typeParameter") == 0) return SEM_TYPE;
    if (strcmp(name, "function") == 0) return SEM_FUNCTION;
    if (strcmp(name, "method") == 0) return SEM_FUNCTION;
    if (strcmp(name, "variable") == 0) return SEM_VARIABLE;
    if (strcmp(name, "parameter") == 0) return SEM_PARAMETER;
    if (strcmp(name, "property") == 0) return SEM_PROPERTY;
    if (strcmp(name, "enumMember") == 0) return SEM_ENUM;
    if (strcmp(name, "macro") == 0) return SEM_MACRO;

    if (strcmp(name, "string") == 0) return SEM_STRING;
    if (strcmp(name, "number") == 0) return SEM_NUMBER;
    if (strcmp(name, "comment") == 0) return SEM_COMMENT;

    if (strcmp(name, "namespace") == 0) return SEM_NAMESPACE;

    if (strcmp(name, "operator") == 0) return SEM_OPERATOR;

    return SEM_NONE;
}