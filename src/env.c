#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "env.h"
#include "sys.h"
#include "utils.h"
#include "quickjs.h"

// ENV file configuration — chaining hash table for O(1) lookup

#define ENV_HASH_SIZE 64

typedef struct EnvNode {
    char *key;
    char *value;
    struct EnvNode *next;
} EnvNode;

static EnvNode *g_hash[ENV_HASH_SIZE];

static unsigned int env_hash_fn(const char *s) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h % ENV_HASH_SIZE;
}

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

// add or update key in hash table
void env_add(const char *key, const char *value) {
    unsigned int idx = env_hash_fn(key);
    EnvNode *n = g_hash[idx];
    while (n) {
        if (strcmp(n->key, key) == 0) {
            char *new_val = strdup(value);
            if (new_val) { free(n->value); n->value = new_val; }
            return;
        }
        n = n->next;
    }
    EnvNode *node = malloc(sizeof(EnvNode));
    if (!node) return;
    node->key = strdup(key);
    node->value = strdup(value);
    if (!node->key || !node->value) {
        free(node->key); free(node->value); free(node);
        return;
    }
    node->next = g_hash[idx];
    g_hash[idx] = node;
}

// O(1) lookup
const char *env_get(const char *key, const char *def) {
    unsigned int idx = env_hash_fn(key);
    EnvNode *n = g_hash[idx];
    while (n) {
        if (strcmp(n->key, key) == 0) return n->value;
        n = n->next;
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
                    char pathbuf[PATH_MAX];
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
        char pathbuf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf) - 1);
        if (len != -1) {
            pathbuf[len] = '\0';
            if (strcmp(stored, pathbuf) != 0) {
                env_add("jssh_loc", pathbuf);

                // rewrite env file by iterating hash table
                f = fopen(filename, "w");
                if (f) {
                    for (int i = 0; i < ENV_HASH_SIZE; i++) {
                        for (EnvNode *n = g_hash[i]; n; n = n->next) {
                            fprintf(f, "%s=%s\n", n->key, n->value);
                        }
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
