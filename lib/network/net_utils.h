#ifndef NET_UTILS_H
#define NET_UTILS_H

#include "quickjs.h"

JSValue js_ssh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_route(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_tracert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_ifconfig(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_net_ping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_net_netstat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
