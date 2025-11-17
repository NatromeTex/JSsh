// lib/fs/module.c
#include "quickjs.h"
#include "fs_utils.h"

void js_init_fs(JSContext *ctx) {
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue fs = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, fs, "df", JS_NewCFunction(ctx, js_df, "df", 0));
  JS_SetPropertyStr(ctx, fs, "find", JS_NewCFunction(ctx, js_find, "find", 0));
  JS_SetPropertyStr(ctx, fs, "tree", JS_NewCFunction(ctx, js_tree, "tree", 0));
  JS_SetPropertyStr(ctx, global_obj, "fs", fs);

  JS_FreeValue(ctx, global_obj);
}