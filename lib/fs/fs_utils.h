#ifndef FS_UTILS_H
#define FS_UTILS_H

#include "quickjs.h"

JSValue js_df(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_tree(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif