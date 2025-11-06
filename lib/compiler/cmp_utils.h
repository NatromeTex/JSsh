#ifndef CMP_UTILS_H
#define CMP_UTILS_H

#include "quickjs.h"

typedef struct {
    const char *name;
    char *version;
} CompilerInfo;

extern CompilerInfo detected[];
extern int detected_count;

void detect_compilers(void);
JSValue js_compiler_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_run_compiler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic,JSValueConst *func_data);

#endif