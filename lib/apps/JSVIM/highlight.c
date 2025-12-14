// highlight.c - Syntax highlighting functions
#include "highlight.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// C/C++ Highlight Rules
// ============================================================================

static HighlightRule c_rules[] = {
    // Line comments (must come before operators to catch //)
    { "//.*$", SEM_COMMENT, HL_FLAG_NONE, {0}, 0 },
    
    // Preprocessor directives
    { "^[ \t]*#[ \t]*(include|define|undef|ifdef|ifndef|if|else|elif|endif|pragma|error|warning)\\b",
      SEM_MACRO, HL_FLAG_NONE, {0}, 0 },
    
    // String literals (double quotes)
    { "\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Character literals (single quotes)
    { "'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Keywords
    { "\\b(auto|break|case|const|continue|default|do|else|enum|extern|for|goto|if|inline|register|restrict|return|sizeof|static|struct|switch|typedef|union|volatile|while|_Alignas|_Alignof|_Atomic|_Bool|_Complex|_Generic|_Imaginary|_Noreturn|_Static_assert|_Thread_local)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // C++ additional keywords
    { "\\b(alignas|alignof|and|and_eq|asm|bitand|bitor|catch|class|compl|concept|consteval|constexpr|constinit|const_cast|co_await|co_return|co_yield|decltype|delete|dynamic_cast|explicit|export|friend|mutable|namespace|new|noexcept|not|not_eq|nullptr|operator|or|or_eq|override|private|protected|public|reinterpret_cast|requires|static_assert|static_cast|template|this|thread_local|throw|try|typeid|typename|using|virtual|xor|xor_eq)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Types
    { "\\b(void|char|short|int|long|float|double|signed|unsigned|bool|size_t|ssize_t|int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|intptr_t|uintptr_t|ptrdiff_t|wchar_t|char16_t|char32_t|FILE|NULL)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Numbers (hex, octal, binary, float, decimal)
    { "\\b0[xX][0-9a-fA-F]+[uUlL]*\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[bB][01]+[uUlL]*\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[0-7]+[uUlL]*\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9]+\\.[0-9]*([eE][+-]?[0-9]+)?[fFlL]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9]+[uUlL]*\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
};

static LanguageHighlighter c_highlighter = {
    .ft = FT_C,
    .rules = c_rules,
    .rule_count = sizeof(c_rules) / sizeof(c_rules[0]),
    .block_comment_start = "/*",
    .block_comment_end = "*/"
};

static LanguageHighlighter cpp_highlighter = {
    .ft = FT_CPP,
    .rules = c_rules,  // Reuse C rules (includes C++ keywords)
    .rule_count = sizeof(c_rules) / sizeof(c_rules[0]),
    .block_comment_start = "/*",
    .block_comment_end = "*/"
};

// ============================================================================
// Python Highlight Rules
// ============================================================================

static HighlightRule python_rules[] = {
    // Line comments
    { "#.*$", SEM_COMMENT, HL_FLAG_NONE, {0}, 0 },
    
    // Triple-quoted strings (must come before single-line strings)
    { "\"\"\"([^\"]|\"[^\"]|\"\"[^\"])*\"\"\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    { "'''([^']|'[^']|''[^'])*'''", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // F-strings, raw strings, byte strings prefixes
    { "[fFrRbBuU]+\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    { "[fFrRbBuU]+'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Regular strings
    { "\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    { "'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Keywords
    { "\\b(and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in constants
    { "\\b(True|False|None|Ellipsis|NotImplemented|__debug__)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in types
    { "\\b(int|float|complex|str|bytes|bytearray|list|tuple|dict|set|frozenset|bool|object|type|range|slice|memoryview|super)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in functions
    { "\\b(abs|all|any|ascii|bin|breakpoint|callable|chr|classmethod|compile|delattr|dir|divmod|enumerate|eval|exec|filter|format|getattr|globals|hasattr|hash|help|hex|id|input|isinstance|issubclass|iter|len|locals|map|max|min|next|oct|open|ord|pow|print|property|repr|reversed|round|setattr|sorted|staticmethod|sum|vars|zip|__import__)\\b",
      SEM_FUNCTION, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in exceptions
    { "\\b(BaseException|Exception|ArithmeticError|AssertionError|AttributeError|BlockingIOError|BrokenPipeError|BufferError|BytesWarning|ChildProcessError|ConnectionAbortedError|ConnectionError|ConnectionRefusedError|ConnectionResetError|DeprecationWarning|EOFError|EnvironmentError|FileExistsError|FileNotFoundError|FloatingPointError|FutureWarning|GeneratorExit|IOError|ImportError|ImportWarning|IndentationError|IndexError|InterruptedError|IsADirectoryError|KeyError|KeyboardInterrupt|LookupError|MemoryError|ModuleNotFoundError|NameError|NotADirectoryError|NotImplementedError|OSError|OverflowError|PendingDeprecationWarning|PermissionError|ProcessLookupError|RecursionError|ReferenceError|ResourceWarning|RuntimeError|RuntimeWarning|StopAsyncIteration|StopIteration|SyntaxError|SyntaxWarning|SystemError|SystemExit|TabError|TimeoutError|TypeError|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError|UnicodeTranslateError|UnicodeWarning|UserWarning|ValueError|Warning|ZeroDivisionError)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Decorators
    { "@[a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)*", SEM_MACRO, HL_FLAG_NONE, {0}, 0 },
    
    // self and cls parameters
    { "\\b(self|cls)\\b", SEM_PARAMETER, HL_FLAG_NONE, {0}, 0 },
    
    // Numbers (hex, octal, binary, float, complex, decimal)
    { "\\b0[xX][0-9a-fA-F_]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[oO][0-7_]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[bB][01_]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9][0-9_]*\\.[0-9_]*([eE][+-]?[0-9_]+)?[jJ]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9][0-9_]*[jJ]\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9][0-9_]*\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
};

static LanguageHighlighter python_highlighter = {
    .ft = FT_PYTHON,
    .rules = python_rules,
    .rule_count = sizeof(python_rules) / sizeof(python_rules[0]),
    .block_comment_start = NULL,  // Python uses triple quotes, handled by regex
    .block_comment_end = NULL
};

// ============================================================================
// Java Highlight Rules
// ============================================================================

static HighlightRule java_rules[] = {
    // Line comments
    { "//.*$", SEM_COMMENT, HL_FLAG_NONE, {0}, 0 },
    
    // String literals (double quotes)
    { "\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Character literals (single quotes)
    { "'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Text blocks (triple double quotes - Java 15+)
    { "\"\"\"([^\"]|\"[^\"]|\"\"[^\"])*\"\"\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Keywords
    { "\\b(abstract|assert|break|case|catch|class|const|continue|default|do|else|enum|extends|final|finally|for|goto|if|implements|import|instanceof|interface|native|new|package|private|protected|public|return|static|strictfp|super|switch|synchronized|this|throw|throws|transient|try|volatile|while|yield)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Modern Java keywords (Java 9+)
    { "\\b(exports|module|non-sealed|open|opens|permits|provides|record|requires|sealed|to|transitive|uses|var|with)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Primitive types
    { "\\b(boolean|byte|char|double|float|int|long|short|void)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Common wrapper and utility classes
    { "\\b(Boolean|Byte|Character|Class|Double|Enum|Float|Integer|Long|Number|Object|Short|String|StringBuffer|StringBuilder|System|Thread|Throwable|Void)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Collection types
    { "\\b(ArrayList|Collection|Collections|HashMap|HashSet|Hashtable|Iterator|LinkedHashMap|LinkedHashSet|LinkedList|List|Map|Queue|Set|Stack|TreeMap|TreeSet|Vector)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Common exception types
    { "\\b(Exception|RuntimeException|Error|Throwable|ArithmeticException|ArrayIndexOutOfBoundsException|ClassCastException|ClassNotFoundException|CloneNotSupportedException|IllegalArgumentException|IllegalStateException|IndexOutOfBoundsException|InterruptedException|IOException|NullPointerException|NumberFormatException|OutOfMemoryError|SecurityException|StackOverflowError|UnsupportedOperationException)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Boolean literals and null
    { "\\b(true|false|null)\\b", SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Annotations
    { "@[a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)*", SEM_MACRO, HL_FLAG_NONE, {0}, 0 },
    
    // Numbers (hex, binary, octal, float, long, decimal)
    { "\\b0[xX][0-9a-fA-F_]+[lL]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[bB][01_]+[lL]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[0-7_]+[lL]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9][0-9_]*\\.[0-9_]*([eE][+-]?[0-9_]+)?[fFdD]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9][0-9_]*[lLfFdD]?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
};

static LanguageHighlighter java_highlighter = {
    .ft = FT_JAVA,
    .rules = java_rules,
    .rule_count = sizeof(java_rules) / sizeof(java_rules[0]),
    .block_comment_start = "/*",
    .block_comment_end = "*/"
};

// Array of all highlighters
static LanguageHighlighter *all_highlighters[] = {
    &c_highlighter,
    &cpp_highlighter,
    &python_highlighter,
    &java_highlighter,
};
static const size_t highlighter_count = sizeof(all_highlighters) / sizeof(all_highlighters[0]);

// ============================================================================
// Core Functions
// ============================================================================

LanguageHighlighter *get_highlighter(FileType ft) {
    for (size_t i = 0; i < highlighter_count; i++) {
        if (all_highlighters[i]->ft == ft)
            return all_highlighters[i];
    }
    return NULL;
}

// Compile all regexes for a highlighter (lazy initialization)
static void compile_rules(LanguageHighlighter *hl) {
    for (size_t i = 0; i < hl->rule_count; i++) {
        HighlightRule *rule = &hl->rules[i];
        if (!rule->is_compiled && rule->pattern) {
            int flags = REG_EXTENDED;
            if (regcomp(&rule->compiled, rule->pattern, flags) == 0) {
                rule->is_compiled = 1;
            }
        }
    }
}

// Check if position is already covered by a token
static int position_has_token(Buffer *buf, int line, int col) {
    for (size_t i = 0; i < buf->token_count; i++) {
        SemanticToken *t = &buf->tokens[i];
        if (t->line == line && col >= t->col && col < t->col + t->len)
            return 1;
    }
    return 0;
}

// Highlight a single line with regex rules
static void highlight_line(Buffer *buf, LanguageHighlighter *hl, int lineno, int *in_block_comment) {
    if (lineno < 0 || (size_t)lineno >= buf->count)
        return;
    
    const char *line = buf->lines[lineno];
    if (!line) return;
    
    size_t line_len = strlen(line);
    
    // Handle block comments first (state carried across lines)
    if (*in_block_comment) {
        const char *end = strstr(line, hl->block_comment_end);
        if (end) {
            // Block comment ends on this line
            int end_col = (int)(end - line) + (int)strlen(hl->block_comment_end);
            SemanticToken tok = { lineno, 0, end_col, SEM_COMMENT, 0, TOKEN_SOURCE_REGEX };
            semantic_token_push(buf, &tok);
            *in_block_comment = 0;
            // Continue highlighting rest of line after comment
        } else {
            // Entire line is in block comment
            SemanticToken tok = { lineno, 0, (int)line_len, SEM_COMMENT, 0, TOKEN_SOURCE_REGEX };
            semantic_token_push(buf, &tok);
            return;
        }
    }
    
    // Check for block comment start
    if (hl->block_comment_start) {
        const char *start = line;
        while ((start = strstr(start, hl->block_comment_start)) != NULL) {
            int start_col = (int)(start - line);
            
            // Skip if inside a string (crude check - position already tokenized)
            if (position_has_token(buf, lineno, start_col)) {
                start++;
                continue;
            }
            
            const char *end = strstr(start + strlen(hl->block_comment_start), hl->block_comment_end);
            if (end) {
                // Block comment starts and ends on same line
                int len = (int)(end - start) + (int)strlen(hl->block_comment_end);
                SemanticToken tok = { lineno, start_col, len, SEM_COMMENT, 0, TOKEN_SOURCE_REGEX };
                semantic_token_push(buf, &tok);
                start = end + strlen(hl->block_comment_end);
            } else {
                // Block comment starts here and continues to next line
                SemanticToken tok = { lineno, start_col, (int)(line_len - start_col), SEM_COMMENT, 0, TOKEN_SOURCE_REGEX };
                semantic_token_push(buf, &tok);
                *in_block_comment = 1;
                return;
            }
        }
    }
    
    // Apply regex rules
    for (size_t r = 0; r < hl->rule_count; r++) {
        HighlightRule *rule = &hl->rules[r];
        if (!rule->is_compiled)
            continue;
        
        regmatch_t match;
        const char *p = line;
        int offset = 0;
        
        while (regexec(&rule->compiled, p, 1, &match, (offset > 0 ? REG_NOTBOL : 0)) == 0) {
            int col = offset + match.rm_so;
            int len = match.rm_eo - match.rm_so;
            
            if (len <= 0) {
                p++;
                offset++;
                continue;
            }
            
            // Skip if this position is already covered (e.g., by block comment or higher-priority rule)
            if (!position_has_token(buf, lineno, col)) {
                SemanticToken tok = { lineno, col, len, rule->kind, 0, TOKEN_SOURCE_REGEX };
                semantic_token_push(buf, &tok);;
            }
            
            p = line + col + len;
            offset = col + len;
            
            // Prevent infinite loop
            if (*p == '\0')
                break;
        }
    }
}

void highlight_buffer(Buffer *buf) {
    if (!buf) return;
    
    LanguageHighlighter *hl = get_highlighter(buf->ft);
    if (!hl) return;
    
    // Compile regexes if not already done
    compile_rules(hl);
    
    // Clear only regex tokens (preserve any LSP tokens)
    semantic_tokens_clear_regex(buf);
    
    // Highlight all lines
    int in_block_comment = 0;
    for (size_t i = 0; i < buf->count; i++) {
        highlight_line(buf, hl, (int)i, &in_block_comment);
    }
}

void highlight_cleanup(void) {
    for (size_t h = 0; h < highlighter_count; h++) {
        LanguageHighlighter *hl = all_highlighters[h];
        for (size_t r = 0; r < hl->rule_count; r++) {
            if (hl->rules[r].is_compiled) {
                regfree(&hl->rules[r].compiled);
                hl->rules[r].is_compiled = 0;
            }
        }
    }
}

// ============================================================================
// Existing Functions (unchanged)
// ============================================================================

void semantic_tokens_clear(Buffer *buf) {
    if (!buf) return;
    buf->token_count = 0;
}

void semantic_tokens_clear_regex(Buffer *buf) {
    if (!buf) return;
    // Remove only regex tokens, keep LSP tokens
    size_t write_idx = 0;
    for (size_t i = 0; i < buf->token_count; i++) {
        if (buf->tokens[i].source == TOKEN_SOURCE_LSP) {
            buf->tokens[write_idx++] = buf->tokens[i];
        }
    }
    buf->token_count = write_idx;
}

void semantic_tokens_clear_lsp(Buffer *buf) {
    if (!buf) return;
    // Remove only LSP tokens, keep regex tokens
    size_t write_idx = 0;
    for (size_t i = 0; i < buf->token_count; i++) {
        if (buf->tokens[i].source == TOKEN_SOURCE_REGEX) {
            buf->tokens[write_idx++] = buf->tokens[i];
        }
    }
    buf->token_count = write_idx;
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
    SemanticKind regex_kind = SEM_NONE;
    
    for (size_t i = 0; i < buf->token_count; i++) {
        SemanticToken *t = &buf->tokens[i];
        if (t->line != line)
            continue;

        if (col >= t->col && col < t->col + t->len) {
            // LSP tokens take priority - return immediately
            if (t->source == TOKEN_SOURCE_LSP)
                return t->kind;
            // Remember regex match as fallback
            if (regex_kind == SEM_NONE)
                regex_kind = t->kind;
        }
    }
    return regex_kind;
}
