# JSVIM - A Terminal Text Editor for JSSH

JSVIM is a lightweight terminal-based text editor built with ncurses, designed as part of the JSSH project. It provides vim-like functionality with modern features including syntax highlighting and LSP (Language Server Protocol) support for enhanced code intelligence.

## Features

- **Modal Editing**: Insert and command modes similar to vim
- **Syntax Highlighting**: Regex-based highlighting for multiple languages
_ **Autosave**: Autosaves modified buffer every 2 seconds
- **LSP Integration**: Deep semantic highlighting for C/C++ via clangd
- **Configurable LSP Servers**: User-defined language servers via `~/.jsvimrc`
- **File Type Detection**: Automatic language detection based on file extension
- **Block Comment Support**: Proper handling of multi-line comments

## Supported Languages
These language servers have been tested for compatibility with JSVIM

| Language   | File Extensions                           | LSP Support |
|------------|-------------------------------------------|-------------|
| C          | `.c`, `.h`                                | ✓ (clangd)  |
| C++        | `.cpp`, `.cc`, `.cxx`, `.hpp`, `.hxx`, `.hh` | ✓ (clangd)  |
| JavaScript | `.js`, `.mjs`, `.cjs`, `.jsx`             | ✓ (typescript-language-server) |
| TypeScript | `.ts`, `.tsx`                             | ✓ (typescript-language-server) |
| Python     | `.py`, `.pyw`, `.pyi`                     | ✓ (pyright-langserver) |
| Rust       | `.rs`                                     | ✗           |
| Go         | `.go`                                     | ✗           |
| Java       | `.java`                                   | ✗           |
| Shell      | `.sh`, `.bash`, `.zsh`                    | ✗           |
| Makefile   | `Makefile`, `makefile`, `GNUmakefile`, `.mk` | ✗        |
| JSON       | `.json`                                   | ✗           |
| Markdown   | `.md`, `.markdown`                        | ✗           |

## Architecture Overview

```
JSVIM/
├── main.c        # Entry point and main loop
├── editor.c/h    # Editor state and key handling
├── buffer.c/h    # Text buffer management
├── render.c/h    # ncurses rendering
├── language.c/h  # File type detection
├── highlight.c/h # Syntax highlighting engine
├── semantic.c/h  # Semantic token types
├── lsp.c/h       # Language Server Protocol client
└── util.c/h      # Common utilities
```

## Configuration

JSVIM can be configured via a `~/.jsvimrc` file in your home directory. This file allows you to customize LSP servers for each language, and also define editor level settings like tab width.

### Configuration File Format

The config file uses a simple `key=value` format:

```ini
# ~/.jsvimrc - JSVIM Configuration

# LSP server configurations
# Format: lsp.<language>=<command> [args...]

lsp.c=clangd
lsp.cpp=clangd
lsp.python=pyright-langserver --stdio
lsp.javascript=typescript-language-server --stdio
lsp.typescript=typescript-language-server --stdio
lsp.rust=rust-analyzer
lsp.go=gopls
```

### Example Configurations

**Using pylsp instead of pyright:**
```ini
lsp.python=pylsp
```

**Using custom clangd path:**
```ini
lsp.c=/usr/local/bin/clangd --background-index
lsp.cpp=/usr/local/bin/clangd --background-index
```

**Enabling rust-analyzer:**
```ini
lsp.rust=rust-analyzer
```

**Using deno for JavaScript/TypeScript:**
```ini
lsp.javascript=deno lsp
lsp.typescript=deno lsp
```

**Customizing semantic colors:**
```ini
# Semantic token colors (ncurses color indexes; -1 = default)
editor.color.keyword=4      # COLOR_BLUE
editor.color.type=6         # COLOR_CYAN
editor.color.function=3     # COLOR_YELLOW
editor.color.string=127
editor.color.number=14
editor.color.comment=34
editor.color.operator=7     # COLOR_WHITE
editor.color.macro=5        # COLOR_MAGENTA
editor.color.class=2        # COLOR_GREEN
editor.color.enum=2
editor.color.namespace=66
editor.color.variable=7
editor.color.parameter=180
editor.color.property=110
```

> **Note:** If `~/.jsvimrc` does not exist or a language is not configured, JSVIM falls back to built-in defaults. It also creates the file on startup

## Highlighting System

JSVIM uses a two-tier highlighting system:

1. **Regex-based highlighting** - Fast, pattern-matching based syntax highlighting
2. **LSP semantic highlighting** - Deep, context-aware highlighting from language servers

LSP tokens take priority over regex tokens when both are available, providing more accurate highlighting for complex code.

---

# Adding New Language Rulesets

This guide explains how to add syntax highlighting support for new languages in JSVIM.

## Step 1: Add FileType Enum

In [language.h](language.h), add a new entry to the `FileType` enum:

```c
typedef enum {
    FT_NONE = 0,
    FT_C,
    FT_CPP,
    FT_JS,
    FT_PYTHON,
    // ... existing types ...
    FT_RUBY,      // <-- Add your new language here
} FileType;
```

## Step 2: Add File Extension Detection

In [language.c](language.c), update the `detect_filetype()` function to recognize your language's file extensions:

```c
FileType detect_filetype(const char *filename) {
    // ... existing code ...
    
    // Ruby
    if (strcmp(ext, "rb") == 0 || strcmp(ext, "rake") == 0 ||
        strcmp(ext, "gemspec") == 0)
        return FT_RUBY;
    
    return FT_NONE;
}
```

For special filenames without extensions (like `Rakefile`), add detection by basename:

```c
// Check for extensionless files by basename
const char *basename = strrchr(filename, '/');
basename = basename ? basename + 1 : filename;

if (strcmp(basename, "Rakefile") == 0)
    return FT_RUBY;
```

## Step 3: Create Highlight Rules

In [highlight.c](highlight.c), create an array of `HighlightRule` structures for your language. Each rule maps a regex pattern to a `SemanticKind`:

```c
// ============================================================================
// Ruby Highlight Rules
// ============================================================================

static HighlightRule ruby_rules[] = {
    // Line comments
    { "#.*$", SEM_COMMENT, HL_FLAG_NONE, {0}, 0 },
    
    // String literals (double quotes)
    { "\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // String literals (single quotes)
    { "'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Keywords
    { "\\b(def|class|module|end|if|else|elsif|unless|case|when|while|until|for|do|begin|rescue|ensure|raise|return|yield|break|next|redo|retry|self|super|nil|true|false|and|or|not|in|then|alias|defined\\?)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in types/classes
    { "\\b(Array|Hash|String|Integer|Float|Symbol|Proc|Lambda|Range|Regexp|Object|Class|Module|NilClass|TrueClass|FalseClass)\\b",
      SEM_TYPE, HL_FLAG_NONE, {0}, 0 },
    
    // Symbols
    { ":[a-zA-Z_][a-zA-Z0-9_]*", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Instance variables
    { "@[a-zA-Z_][a-zA-Z0-9_]*", SEM_VARIABLE, HL_FLAG_NONE, {0}, 0 },
    
    // Class variables
    { "@@[a-zA-Z_][a-zA-Z0-9_]*", SEM_VARIABLE, HL_FLAG_NONE, {0}, 0 },
    
    // Numbers
    { "\\b[0-9]+\\.[0-9]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0x[0-9a-fA-F]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
};
```

### Available SemanticKind Values

| Kind | Description | Typical Color |
|------|-------------|---------------|
| `SEM_KEYWORD` | Language keywords | Blue/Purple |
| `SEM_TYPE` | Types, classes, structs | Green |
| `SEM_FUNCTION` | Function/method names | Yellow |
| `SEM_VARIABLE` | Variables | Blue |
| `SEM_PARAMETER` | Function parameters | Blue |
| `SEM_PROPERTY` | Object properties | Yellow |
| `SEM_MACRO` | Preprocessor macros | Yellow |
| `SEM_STRING` | String literals | Orange/Red |
| `SEM_NUMBER` | Numeric literals | Cyan |
| `SEM_COMMENT` | Comments | Gray |
| `SEM_NAMESPACE` | Namespaces | Green |
| `SEM_CLASS` | Class names | Green |
| `SEM_STRUCT` | Struct names | Green |
| `SEM_ENUM` | Enum names | Green |
| `SEM_OPERATOR` | Operators | Default |

## Step 4: Create Language Highlighter

Create a `LanguageHighlighter` struct that ties your rules together:

```c
static LanguageHighlighter ruby_highlighter = {
    .ft = FT_RUBY,
    .rules = ruby_rules,
    .rule_count = sizeof(ruby_rules) / sizeof(ruby_rules[0]),
    .block_comment_start = "=begin",  // Ruby's block comment start
    .block_comment_end = "=end"       // Ruby's block comment end
};
```

For languages without block comments, set these to `NULL`:

```c
.block_comment_start = NULL,
.block_comment_end = NULL
```

## Step 5: Register the Highlighter

Add your highlighter to the `all_highlighters` array in [highlight.c](highlight.c):

```c
static LanguageHighlighter *all_highlighters[] = {
    &c_highlighter,
    &cpp_highlighter,
    &ruby_highlighter,  // <-- Add your highlighter here
};
```

The `highlighter_count` is calculated automatically using `sizeof()`.

## Writing Good Regex Patterns

### Tips

1. **Order matters**: Rules are applied in order. Put more specific patterns (like line comments) before general ones (like operators).

2. **Word boundaries**: Use `\\b` for word boundaries to avoid partial matches:
   ```c
   "\\bif\\b"  // Matches "if" but not "endif" or "iffy"
   ```

3. **Escape properly**: In C strings, backslashes must be doubled:
   ```c
   "\\b[0-9]+\\b"  // Regex: \b[0-9]+\b
   ```

4. **Character classes**: Use `[...]` for character sets:
   ```c
   "[a-zA-Z_][a-zA-Z0-9_]*"  // Identifier pattern
   ```

5. **Alternation**: Use `|` for alternatives:
   ```c
   "\\b(if|else|while|for)\\b"
   ```

6. **Line anchors**: Use `^` for start of line, `$` for end:
   ```c
   "^[ \\t]*#"   // Matches preprocessor directives at line start
   "//.*$"       // Matches line comments to end of line
   ```

### Common Patterns

```c
// Line comments (// style)
"//.*$"

// Line comments (# style)
"#.*$"

// Double-quoted strings (with escape handling)
"\"([^\"\\\\]|\\\\.)*\""

// Single-quoted strings
"'([^'\\\\]|\\\\.)*'"

// Identifiers
"[a-zA-Z_][a-zA-Z0-9_]*"

// Decimal numbers
"\\b[0-9]+\\b"

// Floating point numbers
"\\b[0-9]+\\.[0-9]+([eE][+-]?[0-9]+)?\\b"

// Hexadecimal numbers
"\\b0[xX][0-9a-fA-F]+\\b"
```

## Complete Example: Adding Lua Support

Here's a complete example adding Lua language support:

### 1. In language.h:
```c
typedef enum {
    // ... existing types ...
    FT_LUA,
} FileType;
```

### 2. In language.c:
```c
// Lua
if (strcmp(ext, "lua") == 0)
    return FT_LUA;
```

### 3. In highlight.c:
```c
// ============================================================================
// Lua Highlight Rules
// ============================================================================

static HighlightRule lua_rules[] = {
    // Line comments
    { "--.*$", SEM_COMMENT, HL_FLAG_NONE, {0}, 0 },
    
    // Strings
    { "\"([^\"\\\\]|\\\\.)*\"", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    { "'([^'\\\\]|\\\\.)*'", SEM_STRING, HL_FLAG_NONE, {0}, 0 },
    
    // Keywords
    { "\\b(and|break|do|else|elseif|end|false|for|function|goto|if|in|local|nil|not|or|repeat|return|then|true|until|while)\\b",
      SEM_KEYWORD, HL_FLAG_NONE, {0}, 0 },
    
    // Built-in functions
    { "\\b(print|type|tonumber|tostring|pairs|ipairs|next|select|unpack|rawget|rawset|setmetatable|getmetatable|require|dofile|loadfile|pcall|xpcall|error|assert)\\b",
      SEM_FUNCTION, HL_FLAG_NONE, {0}, 0 },
    
    // Numbers
    { "\\b[0-9]+\\.[0-9]*([eE][+-]?[0-9]+)?\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b0[xX][0-9a-fA-F]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
    { "\\b[0-9]+\\b", SEM_NUMBER, HL_FLAG_NONE, {0}, 0 },
};

static LanguageHighlighter lua_highlighter = {
    .ft = FT_LUA,
    .rules = lua_rules,
    .rule_count = sizeof(lua_rules) / sizeof(lua_rules[0]),
    .block_comment_start = "--[[",
    .block_comment_end = "]]"
};

// Add to all_highlighters array:
static LanguageHighlighter *all_highlighters[] = {
    &c_highlighter,
    &cpp_highlighter,
    &lua_highlighter,
};
```

## Testing Your Rules

1. Compile JSVIM with your changes
2. Open a file with your language's extension
3. Check that:
   - Keywords are highlighted correctly
   - Strings are highlighted (including escaped characters)
   - Comments are highlighted (both line and block)
   - Numbers are highlighted
   - No false positives (keywords inside strings shouldn't be highlighted)

## Troubleshooting

- **Pattern not matching**: Check regex escaping (double backslashes in C strings)
- **Wrong tokens highlighted**: Check rule order - put specific patterns before general ones
- **Block comments not working**: Ensure `block_comment_start` and `block_comment_end` are set
- **Compilation errors**: Make sure all struct fields are initialized

---

# Adding LSP Support for a Language

To enable LSP (Language Server Protocol) support for semantic highlighting, diagnostics, and other advanced features, you have two options:

## User Configuration (Recommended)

The easiest way to add LSP support for a language is via the `~/.jsvimrc` configuration file. Simply add a line specifying the LSP command:

```ini
# ~/.jsvimrc
lsp.rust=rust-analyzer
lsp.go=gopls
```

This requires no code changes and takes effect immediately on next file open.

### LspCmd Structure

The `LspCmd` structure is defined in [lsp.h](lsp.h):

```c
typedef struct {
    const char *argv[8];  // NULL-terminated argument array
} LspCmd;
```

- The first element is the executable name
- Subsequent elements are command-line arguments
- The array **must** be NULL-terminated
- Maximum 8 elements (including NULL terminator)

### Common LSP Servers

| Language   | LSP Server | Command |
|------------|------------|---------|
| C/C++      | clangd     | `{ "clangd", NULL }` |
| Python     | pyright    | `{ "pyright-langserver", "--stdio", NULL }` |
| Python     | pylsp      | `{ "pylsp", NULL }` |
| JavaScript/TypeScript | typescript-language-server | `{ "typescript-language-server", "--stdio", NULL }` |
| Rust       | rust-analyzer | `{ "rust-analyzer", NULL }` |
| Go         | gopls      | `{ "gopls", NULL }` |
| Ruby       | solargraph | `{ "solargraph", "stdio", NULL }` |
| Java       | jdtls      | `{ "jdtls", NULL }` |
| Lua        | lua-language-server | `{ "lua-language-server", NULL }` |

## Step 2: Enable LSP Spawning in main.c

In [main.c](main.c), the LSP is spawned for specific file types. Add your language to the condition:

```c
// Then try to start LSP for deep semantic highlighting (layered on top)
if (ed.buf.ft == FT_C || ed.buf.ft == FT_CPP || ed.buf.ft == FT_RUBY) {
    ed.buf.lsp = spawn_lsp(&ed.buf.ft);
    if (ed.buf.lsp.pid > 0) {
        lsp_initialize(&ed.buf);
    }
}
```

## Step 3: Set the Language ID (If Adding New FileType)

LSP servers require a specific `languageId` in the `textDocument/didOpen` notification. In [lsp.c](lsp.c), the `lsp_language_id()` function maps file types to language IDs:

```c
static const char *lsp_language_id(FileType ft) {
    switch (ft) {
        case FT_C:        return "c";
        case FT_CPP:      return "cpp";
        case FT_JS:       return "javascript";
        case FT_TS:       return "typescript";
        case FT_PYTHON:   return "python";
        case FT_RUST:     return "rust";
        case FT_GO:       return "go";
        case FT_JAVA:     return "java";
        case FT_SH:       return "shellscript";
        case FT_MAKEFILE: return "makefile";
        case FT_JSON:     return "json";
        case FT_MARKDOWN: return "markdown";
        // Add your language here:
        case FT_RUBY:     return "ruby";
        default:          return "plaintext";
    }
}
```

Also add the config key mapping in `config_key_to_filetype()`:

```c
if (strcmp(key, "lsp.ruby") == 0) return FT_RUBY;
```

## LSP Protocol Details

### Initialization Flow

1. **spawn_lsp()** - Forks and executes the LSP server, sets up stdin/stdout pipes
2. **lsp_initialize()** - Sends the `initialize` request with client capabilities
3. Server responds with its capabilities (including semantic token legend)
4. Client sends `initialized` notification
5. **lsp_notify_did_open()** - Sends `textDocument/didOpen` with file contents
6. **lsp_request_semantic_tokens()** - Requests semantic tokens for highlighting

### Message Format

LSP uses JSON-RPC 2.0 with HTTP-like headers:

```
Content-Length: 123\r\n
\r\n
{"jsonrpc":"2.0","method":"...","params":{...}}
```

### Key Functions in lsp.c

| Function | Purpose |
|----------|---------|
| `lsp_load_config()` | Load LSP config from `~/.jsvimrc` |
| `lsp_config_cleanup()` | Free config memory on exit |
| `spawn_lsp()` | Fork and exec LSP server process |
| `stop_lsp()` | Terminate LSP server |
| `lsp_send()` | Send JSON-RPC message with headers |
| `lsp_initialize()` | Send initialize request |
| `lsp_notify_did_open()` | Notify server that file is open |
| `lsp_notify_did_change()` | Notify server of file changes |
| `lsp_request_semantic_tokens()` | Request semantic tokens |
| `try_parse_lsp_message()` | Parse incoming LSP messages |

### Semantic Token Handling

When the LSP responds to `textDocument/semanticTokens/full`, the tokens are parsed and stored in the buffer. The token type indices are mapped to `SemanticKind` values using the legend provided during initialization.

In [semantic.c](semantic.c), the `semantic_kind_from_lsp()` function maps LSP token type strings to internal `SemanticKind` values:

```c
SemanticKind semantic_kind_from_lsp(const char *type) {
    if (strcmp(type, "keyword") == 0)   return SEM_KEYWORD;
    if (strcmp(type, "type") == 0)      return SEM_TYPE;
    if (strcmp(type, "function") == 0)  return SEM_FUNCTION;
    // ... etc
}
```
---

## License

JSVIM is part of the JSSH project. See the main project LICENSE file for details.
