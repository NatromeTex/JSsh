// lib/git/module.c
#include "quickjs.h"
#include "git_utils.h"

void js_init_git(JSContext *ctx) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue git = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, git, "init", JS_NewCFunction(ctx, js_git_init, "init", 0));
    JS_SetPropertyStr(ctx, git, "clone", JS_NewCFunction(ctx, js_git_clone, "clone", 0));
    JS_SetPropertyStr(ctx, git, "config", JS_NewCFunction(ctx, js_git_config, "config", 0));
    JS_SetPropertyStr(ctx, git, "add", JS_NewCFunction(ctx, js_git_add, "add", 0));
    JS_SetPropertyStr(ctx, git, "status", JS_NewCFunction(ctx, js_git_status, "status", 0));
    JS_SetPropertyStr(ctx, git, "commit", JS_NewCFunction(ctx, js_git_commit, "commit", 0));
    JS_SetPropertyStr(ctx, git, "diff", JS_NewCFunction(ctx, js_git_diff, "diff", 0));
    JS_SetPropertyStr(ctx, git, "branch", JS_NewCFunction(ctx, js_git_branch, "branch", 0));
    JS_SetPropertyStr(ctx, git, "checkout", JS_NewCFunction(ctx, js_git_checkout, "checkout", 0));
    JS_SetPropertyStr(ctx, git, "merge", JS_NewCFunction(ctx, js_git_merge, "merge", 0));
    JS_SetPropertyStr(ctx, git, "pull", JS_NewCFunction(ctx, js_git_pull, "pull", 0));
    JS_SetPropertyStr(ctx, git, "fetch", JS_NewCFunction(ctx, js_git_fetch, "fetch", 0));
    JS_SetPropertyStr(ctx, git, "push", JS_NewCFunction(ctx, js_git_push, "push", 0));
    JS_SetPropertyStr(ctx, git, "log", JS_NewCFunction(ctx, js_git_log, "log", 0));
    JS_SetPropertyStr(ctx, git, "stash", JS_NewCFunction(ctx, js_git_stash, "stash", 0));
    JS_SetPropertyStr(ctx, global_obj, "git", git);
}