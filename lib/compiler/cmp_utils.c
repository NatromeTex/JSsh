#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "quickjs.h"

#define MAX_COMPILERS 256
#define MAX_PATH_LEN 1024
#define MAX_VERSION_LEN 64

typedef struct {
  char name[256];
  char path[MAX_PATH_LEN];
  char version[MAX_VERSION_LEN];
} Compiler;

static Compiler compilers[MAX_COMPILERS];
static int compiler_count = 0;

static int is_exe(const char *path) {
  return access(path, X_OK) == 0;
}

static void probe_version(Compiler *c) {
  int pipefd[2];
  if (pipe(pipefd) < 0) return;

  pid_t pid = fork();
  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    execlp(c->path, c->path, "--version", NULL);
    execlp(c->path, c->path, "-v", NULL);
    execlp(c->path, c->path, "-V", NULL);
    _exit(1);
  }

  close(pipefd[1]);
  char buf[256] = {0};
  ssize_t n = read(pipefd[0], buf, sizeof(buf)-1);
  close(pipefd[0]);
  waitpid(pid, NULL, 0);

  if (n > 0) {
    buf[n] = 0;
    char *p = strstr(buf, " ");
    if (p) strncpy(c->version, p+1, MAX_VERSION_LEN-1);
    else{
      strncpy(c->version, buf, MAX_VERSION_LEN - 1);
      c->version[MAX_VERSION_LEN - 1] = '\0';
    }
  }
}

static void scan_path() {
  const char *path_env = getenv("PATH");
  if (!path_env) return;


  char *paths = strdup(path_env);
  char *saveptr = NULL;
  char *dir = strtok_r(paths, ":", &saveptr);


  while (dir && compiler_count < MAX_COMPILERS) {
    DIR *d = opendir(dir);
    if (d) {
      struct dirent *ent;
      while ((ent = readdir(d)) != NULL) {
        if (compiler_count >= MAX_COMPILERS) break;
        if (ent->d_type != DT_REG && ent->d_type != DT_LNK) continue;
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        if (!is_exe(full)) continue;
        if (strstr(ent->d_name, "python") || strstr(ent->d_name, "gcc") || strstr(ent->d_name, "clang") || strstr(ent->d_name, "javac")) {
          Compiler c = {0};
          snprintf(c.name, sizeof(c.name), "%s", ent->d_name);
          snprintf(c.path, sizeof(c.path), "%s", full);
          probe_version(&c);
          compilers[compiler_count++] = c;
        }
      }
      closedir(d);
    }
    dir = strtok_r(NULL, ":", &saveptr);
  }
  free(paths);
}

JSValue js_compiler_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  scan_path();
  JSValue result = JS_NewObject(ctx);

  for (int i = 0; i < compiler_count; i++) {
    char *base = compilers[i].name;
    char family[32] = {0};
    if (strstr(base, "python")) strcpy(family, "Python");
    else if (strstr(base, "javac")) strcpy(family, "Java");
    else if (strstr(base, "gcc")) strcpy(family, "gcc");
    else if (strstr(base, "clang")) strcpy(family, "clang");
    else strcpy(family, base);

    JSValue family_arr = JS_GetPropertyStr(ctx, result, family);
    if (JS_IsUndefined(family_arr)) {
      family_arr = JS_NewArray(ctx);
    }

    JSValue len_val = JS_GetPropertyStr(ctx, family_arr, "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    JS_SetPropertyUint32(ctx, family_arr, len, JS_NewString(ctx, compilers[i].version[0] ? compilers[i].version : "(unknown)"));
    JS_SetPropertyStr(ctx, result, family, family_arr);
  }
  return result;
}