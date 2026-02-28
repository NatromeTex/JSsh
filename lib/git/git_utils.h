#ifndef GIT_UTILS_H
#define GIT_UTILS_H

#include "quickjs.h"

JSValue js_git_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_config(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_status(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_commit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_diff(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_branch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_checkout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_merge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Additional porcelain-style commands
JSValue js_git_pull(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_git_stash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif