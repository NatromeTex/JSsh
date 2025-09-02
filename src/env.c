#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "env.h"

// ENV file configuration
typedef struct {
    char *key;
    char *value;
} EnvEntry;

static EnvEntry *g_env = NULL;
static size_t g_env_count = 0;

// default settings
static const char *default_env[] = {
    "color_dir={blue}",
    "color_exe={green}",
    "color_link={cyan}",
    "color_fifo={yellow}",
    "color_sock={magenta}",
    "color_chr={red}",
    "color_blk={red}",
    "color_reg={white}",
    NULL
};

// add to env file
void env_add(const char *key, const char *value) {
    g_env = realloc(g_env, (g_env_count + 1) * sizeof(EnvEntry));
    g_env[g_env_count].key = strdup(key);
    g_env[g_env_count].value = strdup(value);
    g_env_count++;
}

// read setting from env file
const char *env_get(const char *key, const char *def) {
    for (size_t i = 0; i < g_env_count; i++) {
        if (strcmp(g_env[i].key, key) == 0)
            return g_env[i].value;
    }
    return def;
}

// call at startup to load the env file
void env_load(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        // file missing → create with defaults
        FILE *f = fopen(filename, "w");
        if (!f) return;
        for (const char **p = default_env; *p; p++) {
            fprintf(f, "%s\n", *p);
            char key[64], val[128];
            if (sscanf(*p, "%63[^=]=%127[^\n]", key, val) == 2) {
                env_add(key, val);
            }
        }
        fclose(f);
        return;
    }

    // load existing file
    FILE *f = fopen(filename, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127[^\n]", key, val) == 2) {
            env_add(key, val);
        }
    }
    fclose(f);
}

// show contents of env file
void env_show(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("cannot open %s: %s\n", filename, strerror(errno));
        return;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        fputs(buf, stdout);
    }
    fclose(f);
}