#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

// Get the repo name
char* get_repo_name(const char *url) {
    const char *last_slash = strrchr(url, '/');
    if (!last_slash) return strdup("repo");
    const char *name = last_slash + 1;
    char *dotgit = strstr(name, ".git");
    size_t len = dotgit ? (size_t)(dotgit - name) : strlen(name);
    char *result = malloc(len + 1);
    strncpy(result, name, len);
    result[len] = '\0';
    return result;
}

int certificate_check_cb(git_cert *cert, int valid, const char *host, void *payload) {
    return 0;
}

int transfer_progress_cb(const git_indexer_progress *stats, void *payload) {
    if (stats->total_objects > 0) {
        // Receiving objects
        if (stats->received_objects < stats->total_objects) {
            int percent = (100 * stats->received_objects) / stats->total_objects;
            printf("\rReceiving objects: %d%% (%u/%u)", 
                   percent,
                   stats->received_objects, 
                   stats->total_objects);
        }
        // Resolving deltas
        else if (stats->total_deltas > 0 && stats->indexed_deltas < stats->total_deltas) {
            int percent = (100 * stats->indexed_deltas) / stats->total_deltas;
            printf("\rResolving deltas: %d%% (%u/%u)", 
                   percent,
                   stats->indexed_deltas, 
                   stats->total_deltas);
        }
        fflush(stdout);
    }
    return 0;
}

char* normalize_git_url(const char *url) {
    if (strncmp(url, "git://", 6) == 0) {
        size_t len = strlen(url) - 6 + 8 + 1;
        char *https_url = malloc(len);
        snprintf(https_url, len, "https://%s", url + 6);
        return https_url;
    }
    return strdup(url);
}

JSValue js_git_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowInternalError(ctx, "Usage: git_clone(<repo_url>, [target_dir])");
    }
    
    const char *base_url = JS_ToCString(ctx, argv[0]);
    if (!base_url) {
        return JS_ThrowInternalError(ctx, "Invalid URL");
    }
    
    char *url = normalize_git_url(base_url);
    const char *path = NULL;
    char *allocated_path = NULL;
    
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        path = JS_ToCString(ctx, argv[1]);
    } else {
        allocated_path = get_repo_name(url);
        path = allocated_path;
    }
    
    git_libgit2_init();
    
    git_repository *repo = NULL;
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    clone_opts.fetch_opts.callbacks.certificate_check = certificate_check_cb;
    clone_opts.fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
    
    printf("Cloning into '%s'...\n", path);
    
    int error = git_clone(&repo, url, path, &clone_opts);
    
    printf("\n");
    
    JSValue result;
    if (error < 0) {
        const git_error *e = git_error_last();
        result = JS_ThrowInternalError(ctx, "Error cloning repository: %s", e && e->message ? e->message : "unknown error");
    } else {
        git_repository_free(repo);
        result = JS_NewString(ctx, "Repository cloned successfully");
    }
    
    git_libgit2_shutdown();
    
    // Clean up
    JS_FreeCString(ctx, base_url);
    free(url);
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        JS_FreeCString(ctx, path);
    }
    if (allocated_path) {
        free(allocated_path);
    }
    
    return result;
}