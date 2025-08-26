#ifndef FUNC_H
#define FUNC_H

#include "quickjs.h"

JSValue js_cat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_echo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_ls(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_cd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
