// lib/compiler/module.c
#include "quickjs.h"
#include "cmp_utils.h"

void js_init_compiler(JSContext *ctx) {
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue cmp = JS_NewObject(ctx);
  if (detected_count == 0)
    detect_compilers();

  JS_SetPropertyStr(ctx, cmp, "auto", JS_NewCFunction(ctx, js_auto_compile, "auto", 0));
  JS_SetPropertyStr(ctx, cmp, "list", JS_NewCFunction(ctx, js_compiler_list, "list", 0));
  for (int i = 0; i < detected_count; i++) {
    const char *name = detected[i].name;
    JS_SetPropertyStr(ctx, cmp, name, JS_NewCFunctionData(ctx, js_run_compiler, 1, 0, 1, (JSValueConst[]){JS_NewString(ctx, name)}));
  }

  JS_SetPropertyStr(ctx, global_obj, "cmp", cmp);

  JS_FreeValue(ctx, global_obj);
}