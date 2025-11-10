// lib/git/module.c
#include "quickjs.h"
#include "git_utils.h"

void js_init_git(JSContext *ctx) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue git = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, git, "init", JS_NewCFunction(ctx, js_git_init, "init", 0));
    JS_SetPropertyStr(ctx, git, "clone", JS_NewCFunction(ctx, js_git_clone, "clone", 0));
    JS_SetPropertyStr(ctx, global_obj, "git", git);
}