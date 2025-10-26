// lib/compiler/module.c
#include "quickjs.h"
#include "cmp_utils.h"

void js_init_compiler(JSContext *ctx) {
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue cmp = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, cmp, "list", JS_NewCFunction(ctx, js_compiler_list, "list", 0));
  JS_SetPropertyStr(ctx, global_obj, "cmp", cmp);

  JS_FreeValue(ctx, global_obj);
}