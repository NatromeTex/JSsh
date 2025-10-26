#ifndef CMP_UTILS_H
#define CMP_UTILS_H

#include "quickjs.h"

JSValue js_compiler_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif