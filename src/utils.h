#ifndef UTILS_H
#define UTILS_H

#include "quickjs.h"

void printR(const char *fmt, ...);
void detect_color_mode(void);
void setColor(int r, int g, int b);
void resetColor(void);
JSValue js_cat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
