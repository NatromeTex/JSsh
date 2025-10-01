#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "env.h"
#include "sys.h"
#include "utils.h"
#include "quickjs.h"

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
    "jssh_loc={}",
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
    FILE *f;

    // if file does not exist → create with defaults
    if (stat(filename, &st) != 0) {
        f = fopen(filename, "w");
        if (!f) return;

        for (const char **p = default_env; *p; p++) {
            char key[64], val[128];
            if (sscanf(*p, "%63[^=]=%127[^\n]", key, val) == 2) {
                if (strcmp(key, "jssh_loc") == 0) {
                    char pathbuf[256];
                    ssize_t len = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf) - 1);
                    if (len != -1) {
                        pathbuf[len] = '\0';
                        env_add(key, pathbuf);
                        fprintf(f, "%s=%s\n", key, pathbuf);
                        continue;
                    }
                }
                env_add(key, val);
                fprintf(f, "%s=%s\n", key, val);
            }
        }
        fclose(f);
        return;
    }

    // load existing file
    f = fopen(filename, "r");
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

    // verify jssh_loc points to this binary, update if wrong
    const char *stored = env_get("jssh_loc", NULL);
    if (stored) {
        char pathbuf[256];
        ssize_t len = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf) - 1);
        if (len != -1) {
            pathbuf[len] = '\0';
            if (strcmp(stored, pathbuf) != 0) {
                // replace with correct location
                env_add("jssh_loc", pathbuf);

                // rewrite env file
                f = fopen(filename, "w");
                if (f) {
                    for (size_t i = 0; i < g_env_count; i++) {
                        fprintf(f, "%s=%s\n", g_env[i].key, g_env[i].value);
                    }
                    fclose(f);
                }
            }
        }
    }
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