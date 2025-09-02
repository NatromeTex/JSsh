#ifndef ENV_H
#define ENV_H

void env_load(const char *filename);
void env_show(const char *filename);
void env_add(const char *key, const char *value);

#endif