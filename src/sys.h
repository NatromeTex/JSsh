#ifndef SYS_H
#define SYS_H

#include "quickjs.h"

// Register all sys bindings
void init_qol_bindings(void);
void js_init_sys(JSContext *ctx);
void env_load(const char *filename);
void env_show(const char *filename);
const char *env_get(const char *key, const char *def);

#endif
