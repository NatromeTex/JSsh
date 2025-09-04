#ifndef FUNC_H
#define FUNC_H

#include "quickjs.h"

JSValue js_cd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_ls(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_rm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_cat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_tac(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_echo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_chmod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_touch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_runfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
