// lib/apps/module.c
#include "quickjs.h"
#include "editor.h"

void js_init_apps(JSContext *ctx) {
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue apps = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, apps, "jsvim", JS_NewCFunction(ctx, jsvim, "jsvim", 0));
  JS_SetPropertyStr(ctx, global_obj, "apps", apps);

  JS_FreeValue(ctx, global_obj);
}