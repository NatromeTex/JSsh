// lib/network/module.c
#include "quickjs.h"
#include "net_utils.h"

void js_init_network(JSContext *ctx) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue net = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, net, "ssh", JS_NewCFunction(ctx, js_ssh, "ssh", 0));
    JS_SetPropertyStr(ctx, net, "route", JS_NewCFunction(ctx, js_route, "route", 0));
    JS_SetPropertyStr(ctx, net, "ping", JS_NewCFunction(ctx, js_net_ping, "ping", 0));
    JS_SetPropertyStr(ctx, net, "tracert", JS_NewCFunction(ctx, js_tracert, "tracert", 0));
    JS_SetPropertyStr(ctx, net, "ifconfig", JS_NewCFunction(ctx, js_ifconfig, "ifconfig", 0));
    JS_SetPropertyStr(ctx, net, "netstat", JS_NewCFunction(ctx, js_net_netstat, "netstat", 0));
    JS_SetPropertyStr(ctx, global_obj, "net", net);

    JS_FreeValue(ctx, global_obj);
}
