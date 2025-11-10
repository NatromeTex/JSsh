#ifndef GIT_UTILS_H
#define GIT_UTILS_H

#include "quickjs.h"

JSValue js_git_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif