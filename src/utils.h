#ifndef UTILS_H
#define UTILS_H

#include "quickjs.h"

void resetColor(void);
void init_history_file();
void detect_color_mode(void);
void printR(const char *fmt, ...);
void setColor(int r, int g, int b);
void print_name(const char *name, mode_t mode);
void load_js_libs(JSContext *ctx, const char *dirpath);
JSValue js_cat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_env_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_env_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_show_env(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
