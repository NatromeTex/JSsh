#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "quickjs.h"

#define JS_SUPPRESS "\x1B[JSSH_SUPPRESS"

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

// callbacks
int transfer_progress_cb(const git_indexer_progress *stats, void *payload) {
    if (stats->total_objects > 0) {
        // Receiving objects phase
        if (stats->received_objects <= stats->total_objects) {
            if (stats->received_bytes > 0) {
                double mb = stats->received_bytes / (1024.0 * 1024.0);
                printf("\rReceiving objects: %d%% (%u/%u), %.2f MiB", 
                       (100 * stats->received_objects) / stats->total_objects,
                       stats->received_objects, 
                       stats->total_objects,
                       mb);
            } else {
                printf("\rReceiving objects: %d%% (%u/%u)", 
                       (100 * stats->received_objects) / stats->total_objects,
                       stats->received_objects, 
                       stats->total_objects);
            }
            fflush(stdout);
        }
        
        if (stats->received_objects == stats->total_objects && 
            stats->indexed_objects == stats->total_objects) {
            double kb = stats->received_bytes / 1024.0;
            printf("\rReceiving objects: 100%% (%u/%u), %.2f KiB | %.2f MiB/s, done.\n",
                   stats->total_objects,
                   stats->total_objects,
                   kb,
                   kb / 1024.0);
            fflush(stdout);
        }
        
        // Resolving deltas phase
        if (stats->total_deltas > 0) {
            printf("\rResolving deltas: %d%% (%u/%u)", 
                   (100 * stats->indexed_deltas) / stats->total_deltas,
                   stats->indexed_deltas, 
                   stats->total_deltas);
            if (stats->indexed_deltas == stats->total_deltas) {
                printf(", done.");
            }
            fflush(stdout);
        }
    }
    return 0;
}

// basic credential callback: use SSH agent or default credentials when possible
int credentials_cb(git_credential **out,
                   const char *url,
                   const char *username_from_url,
                   unsigned int allowed_types,
                   void *payload) {
    (void)url;
    (void)payload;

    if ((allowed_types & GIT_CREDENTIAL_SSH_KEY) && username_from_url) {
        return git_credential_ssh_key_from_agent(out, username_from_url);
    }
    if (allowed_types & GIT_CREDENTIAL_DEFAULT) {
        return git_credential_default_new(out);
    }
    return GIT_PASSTHROUGH;
}

// Change git:// to https://
char* normalize_git_url(const char *url) {
    if (strncmp(url, "git://", 6) == 0) {
        size_t len = strlen(url) - 6 + 8 + 1;
        char *https_url = malloc(len);
        snprintf(https_url, len, "https://%s", url + 6);
        return https_url;
    }
    return strdup(url);
}

// ---------------------------------------------------------

// Common helper: open repository at current directory
static int open_repo_here(git_repository **out_repo) {
    char path[4096];
    if (!getcwd(path, sizeof(path)))
        return GIT_ERROR;
    return git_repository_open(out_repo, path);
}

static int get_current_branch_name(git_repository *repo, char *out, size_t out_len) {
    git_reference *head = NULL;
    int err = git_repository_head(&head, repo);
    if (err < 0)
        return err;

    const char *name = NULL;
    err = git_branch_name(&name, head);
    if (err < 0 || !name) {
        git_reference_free(head);
        return err ? err : GIT_ERROR;
    }

    strncpy(out, name, out_len - 1);
    out[out_len - 1] = '\0';
    git_reference_free(head);
    return 0;
}

static JSValue js_git_error(JSContext *ctx, const char *msg, int err) {
    const git_error *e = git_error_last();
    const char *detail = (e && e->message) ? e->message : "unknown error";
    if (err == 0)
        detail = "unknown error";
    return JS_ThrowInternalError(ctx, "%s: %s", msg, detail);
}

// git.clone(url, path)
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

// git.init(name, path)
JSValue js_git_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowInternalError(ctx, "Usage: git_init(<repo_name>, [target_dir])");
    }
    
    const char *repo_name = JS_ToCString(ctx, argv[0]);
    if (!repo_name) {
        return JS_ThrowInternalError(ctx, "Invalid repository name");
    }
    
    const char *path = NULL;
    char *allocated_path = NULL;
    
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        path = JS_ToCString(ctx, argv[1]);
    } else {
        allocated_path = strdup(repo_name);
        path = allocated_path;
    }
    
    git_libgit2_init();
    
    git_repository *repo = NULL;
    int error = git_repository_init(&repo, path, 0);
    
    JSValue result;
    if (error < 0) {
        const git_error *e = git_error_last();
        result = JS_ThrowInternalError(ctx, "Error initializing repository: %s", e && e->message ? e->message : "unknown error");
    } else {
        git_repository_free(repo);
        result = JS_NewString(ctx, "Repository initialized successfully");
    }
    
    git_libgit2_shutdown();
    
    // Clean up
    JS_FreeCString(ctx, repo_name);
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        JS_FreeCString(ctx, path);
    }
    if (allocated_path) {
        free(allocated_path);
    }
    
    return result;
}

// git.config(key[, value])  - local repo config in CWD
JSValue js_git_config(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowInternalError(ctx, "Usage: git.config(<key>, [value])");

    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key)
        return JS_ThrowInternalError(ctx, "Invalid key");

    git_libgit2_init();

    git_repository *repo = NULL;
    git_config *cfg = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        JS_FreeCString(ctx, key);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_repository_config(&cfg, repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening config", err);
        JS_FreeCString(ctx, key);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    JSValue result;

    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        // set
        const char *value = JS_ToCString(ctx, argv[1]);
        if (!value) {
            git_config_free(cfg);
            git_repository_free(repo);
            JS_FreeCString(ctx, key);
            git_libgit2_shutdown();
            return JS_ThrowInternalError(ctx, "Invalid value");
        }
        err = git_config_set_string(cfg, key, value);
        if (err < 0)
            result = js_git_error(ctx, "Error setting config", err);
        else
            result = JS_NewString(ctx, JS_SUPPRESS);
        JS_FreeCString(ctx, value);
    } else {
        // get
        const char *out = NULL;
        err = git_config_get_string(&out, cfg, key);
        if (err < 0)
            result = js_git_error(ctx, "Error getting config", err);
        else
            result = JS_NewString(ctx, out);
    }

    git_config_free(cfg);
    git_repository_free(repo);
    JS_FreeCString(ctx, key);
    git_libgit2_shutdown();
    return result;
}

// git.add(path)  (path can be file or '.')
JSValue js_git_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowInternalError(ctx, "Usage: git.add(<path>)");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_ThrowInternalError(ctx, "Invalid path");

    git_libgit2_init();

    git_repository *repo = NULL;
    git_index *index = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        JS_FreeCString(ctx, path);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_repository_index(&index, repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening index", err);
        JS_FreeCString(ctx, path);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    if (strcmp(path, ".") == 0) {
        git_strarray paths = {0};
        err = git_index_add_all(index, &paths, 0, NULL, NULL);
    } else {
        err = git_index_add_bypath(index, path);
    }

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error adding path", err);
    else {
        git_index_write(index);
        result = JS_NewString(ctx, JS_SUPPRESS);
    }

    git_index_free(index);
    git_repository_free(repo);
    JS_FreeCString(ctx, path);
    git_libgit2_shutdown();
    return result;
}

// git.status() - prints short status to stdout
JSValue js_git_status(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;

    git_libgit2_init();

    git_repository *repo = NULL;
    git_status_list *status = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        git_libgit2_shutdown();
        return ret;
    }

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

    err = git_status_list_new(&status, repo, &opts);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error getting status", err);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    size_t count = git_status_list_entrycount(status);
    for (size_t i = 0; i < count; i++) {
        const git_status_entry *e = git_status_byindex(status, i);
        char istatus = ' ';
        char wstatus = ' ';

        if (e->status & GIT_STATUS_INDEX_NEW) istatus = 'A';
        else if (e->status & GIT_STATUS_INDEX_MODIFIED) istatus = 'M';
        else if (e->status & GIT_STATUS_INDEX_DELETED) istatus = 'D';

        if (e->status & GIT_STATUS_WT_NEW) wstatus = '?';
        else if (e->status & GIT_STATUS_WT_MODIFIED) wstatus = 'M';
        else if (e->status & GIT_STATUS_WT_DELETED) wstatus = 'D';

        const char *path = e->head_to_index && e->head_to_index->new_file.path ?
                           e->head_to_index->new_file.path :
                           (e->index_to_workdir && e->index_to_workdir->new_file.path ?
                                e->index_to_workdir->new_file.path : "");

        printf("%c%c %s\n", istatus, wstatus, path);
    }

    git_status_list_free(status);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return JS_NewString(ctx, JS_SUPPRESS);
}

// helper to create signature from config or fallback
static int make_signature(git_signature **sig, git_repository *repo) {
    git_config *cfg = NULL;
    const char *name = NULL;
    const char *email = NULL;
    if (git_repository_config(&cfg, repo) == 0) {
        git_config_get_string(&name, cfg, "user.name");
        git_config_get_string(&email, cfg, "user.email");
        git_config_free(cfg);
    }
    if (!name) name = "JSsh";
    if (!email) email = "js@example";
    return git_signature_now(sig, name, email);
}

// git.commit(message)
JSValue js_git_commit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowInternalError(ctx, "Usage: git.commit(<message>)");

    const char *msg = JS_ToCString(ctx, argv[0]);
    if (!msg)
        return JS_ThrowInternalError(ctx, "Invalid message");

    git_libgit2_init();

    git_repository *repo = NULL;
    git_index *index = NULL;
    git_oid tree_oid, commit_oid, parent_oid;
    git_tree *tree = NULL;
    git_commit *parent = NULL;
    git_signature *sig = NULL;

    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        JS_FreeCString(ctx, msg);
        git_libgit2_shutdown();
        return ret;
    }

    if (git_repository_index(&index, repo) < 0) {
        JSValue ret = js_git_error(ctx, "Error opening index", -1);
        JS_FreeCString(ctx, msg);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    if ((err = git_index_write_tree(&tree_oid, index)) < 0) {
        JSValue ret = js_git_error(ctx, "Error writing tree", err);
        JS_FreeCString(ctx, msg);
        git_index_free(index);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    if ((err = git_tree_lookup(&tree, repo, &tree_oid)) < 0) {
        JSValue ret = js_git_error(ctx, "Error looking up tree", err);
        JS_FreeCString(ctx, msg);
        git_index_free(index);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    make_signature(&sig, repo);

    size_t parent_count = 0;
    if (!git_repository_head_unborn(repo) &&
        git_reference_name_to_id(&parent_oid, repo, "HEAD") == 0 &&
        git_commit_lookup(&parent, repo, &parent_oid) == 0) {
        parent_count = 1;
    }

    const git_commit *parents[1];
    if (parent_count == 1)
        parents[0] = parent;

    err = git_commit_create(&commit_oid, repo, "HEAD", sig, sig, NULL,
                            msg, tree, (int)parent_count, parent_count ? parents : NULL);

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error creating commit", err);
    else
        result = JS_NewString(ctx, JS_SUPPRESS);

    git_signature_free(sig);
    git_commit_free(parent);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);
    JS_FreeCString(ctx, msg);
    git_libgit2_shutdown();
    return result;
}

// diff print callback
static int diff_print_cb(const git_diff_delta *delta, const git_diff_hunk *hunk,
                         const git_diff_line *line, void *payload) {
    (void)delta; (void)hunk; (void)payload;
    fputc(line->origin, stdout);
    fwrite(line->content, 1, line->content_len, stdout);
    return 0;
}

// git.diff()
JSValue js_git_diff(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;

    git_libgit2_init();

    git_repository *repo = NULL;
    git_diff *diff = NULL;
    git_tree *head_tree = NULL;
    git_object *head_obj = NULL;

    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        git_libgit2_shutdown();
        return ret;
    }

    if (!git_repository_head_unborn(repo)) {
        if ((err = git_revparse_single(&head_obj, repo, "HEAD")) == 0)
            err = git_tree_lookup(&head_tree, repo, git_object_id(head_obj));
        if (err < 0) {
            JSValue ret = js_git_error(ctx, "Error loading HEAD", err);
            git_object_free(head_obj);
            git_tree_free(head_tree);
            git_repository_free(repo);
            git_libgit2_shutdown();
            return ret;
        }
    }

    err = git_diff_tree_to_workdir_with_index(&diff, repo, head_tree, NULL);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error creating diff", err);
        git_object_free(head_obj);
        git_tree_free(head_tree);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, diff_print_cb, NULL);

    git_diff_free(diff);
    git_object_free(head_obj);
    git_tree_free(head_tree);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return JS_NewString(ctx, JS_SUPPRESS);
}

// git.branch() or git.branch(name)
JSValue js_git_branch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    git_libgit2_init();

    git_repository *repo = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        git_libgit2_shutdown();
        return ret;
    }

    JSValue result = JS_UNDEFINED;

    if (argc >= 1 && JS_IsString(argv[0])) {
        // create branch at HEAD
        const char *name = JS_ToCString(ctx, argv[0]);
        git_object *head = NULL;
        git_commit *commit = NULL;
        git_reference *ref = NULL;

        if (!name) {
            git_repository_free(repo);
            git_libgit2_shutdown();
            return JS_ThrowInternalError(ctx, "Invalid branch name");
        }

        err = git_revparse_single(&head, repo, "HEAD");
        if (err == 0)
            err = git_commit_lookup(&commit, repo, git_object_id(head));
        if (err == 0)
            err = git_branch_create(&ref, repo, name, commit, 0);

        if (err < 0)
            result = js_git_error(ctx, "Error creating branch", err);
        else
            result = JS_NewString(ctx, JS_SUPPRESS);

        git_reference_free(ref);
        git_commit_free(commit);
        git_object_free(head);
        JS_FreeCString(ctx, name);
    } else {
        // list local branches
        git_branch_iterator *it = NULL;
        git_reference *ref = NULL;
        git_branch_t type;

        err = git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL);
        if (err < 0) {
            result = js_git_error(ctx, "Error listing branches", err);
        } else {
            while (git_branch_next(&ref, &type, it) == 0) {
                const char *name = NULL;
                git_branch_name(&name, ref);
                printf("%s\n", name ? name : "(unknown)");
                git_reference_free(ref);
                ref = NULL;
            }
            git_branch_iterator_free(it);
            result = JS_NewString(ctx, JS_SUPPRESS);
        }
    }

    git_repository_free(repo);
    git_libgit2_shutdown();
    return result;
}

// git.checkout(branch)
JSValue js_git_checkout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowInternalError(ctx, "Usage: git.checkout(<branch>)");

    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name)
        return JS_ThrowInternalError(ctx, "Invalid branch name");

    git_libgit2_init();

    git_repository *repo = NULL;
    git_object *treeish = NULL;
    git_reference *ref = NULL;

    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        JS_FreeCString(ctx, name);
        git_libgit2_shutdown();
        return ret;
    }

    char refname[256];
    snprintf(refname, sizeof(refname), "refs/heads/%s", name);

    err = git_reference_lookup(&ref, repo, refname);
    if (err == 0)
        err = git_revparse_single(&treeish, repo, refname);

    JSValue result;
    if (err < 0) {
        result = js_git_error(ctx, "Error resolving branch", err);
    } else {
        git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
        opts.checkout_strategy = GIT_CHECKOUT_SAFE;
        err = git_checkout_tree(repo, treeish, &opts);
        if (err == 0)
            err = git_repository_set_head(repo, refname);
        if (err < 0)
            result = js_git_error(ctx, "Error checking out branch", err);
        else
            result = JS_NewString(ctx, JS_SUPPRESS);
    }

    git_object_free(treeish);
    git_reference_free(ref);
    git_repository_free(repo);
    JS_FreeCString(ctx, name);
    git_libgit2_shutdown();
    return result;
}

// git.merge(branch)
JSValue js_git_merge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowInternalError(ctx, "Usage: git.merge(<branch>)");

    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name)
        return JS_ThrowInternalError(ctx, "Invalid branch name");

    git_libgit2_init();

    git_repository *repo = NULL;
    git_annotated_commit *their_head = NULL;

    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        JS_FreeCString(ctx, name);
        git_libgit2_shutdown();
        return ret;
    }

    char refname[256];
    snprintf(refname, sizeof(refname), "refs/heads/%s", name);

    err = git_annotated_commit_from_revspec(&their_head, repo, refname);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error resolving branch", err);
        JS_FreeCString(ctx, name);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    const git_annotated_commit *heads[] = { their_head };
    err = git_merge(repo, heads, 1, NULL, NULL);

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error merging", err);
    else
        result = JS_NewString(ctx, JS_SUPPRESS);

    git_annotated_commit_free(their_head);
    git_repository_free(repo);
    JS_FreeCString(ctx, name);
    git_libgit2_shutdown();
    return result;
}

// git.fetch([remote])
JSValue js_git_fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *remote_name = "origin";
    const char *remote_js = NULL;

    if (argc >= 1 && JS_IsString(argv[0])) {
        remote_js = JS_ToCString(ctx, argv[0]);
        if (!remote_js)
            return JS_ThrowInternalError(ctx, "Invalid remote name");
        remote_name = remote_js;
    }

    git_libgit2_init();

    git_repository *repo = NULL;
    git_remote *remote = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_remote_lookup(&remote, repo, remote_name);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error looking up remote", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
    opts.callbacks.credentials = credentials_cb;
    opts.callbacks.certificate_check = certificate_check_cb;
    opts.callbacks.transfer_progress = transfer_progress_cb;

    printf("Fetching from '%s'...\n", remote_name);
    err = git_remote_fetch(remote, NULL, &opts, NULL);

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error fetching from remote", err);
    else
        result = JS_NewString(ctx, JS_SUPPRESS);

    git_remote_free(remote);
    git_repository_free(repo);
    if (remote_js) JS_FreeCString(ctx, remote_js);
    git_libgit2_shutdown();
    return result;
}

// git.pull([remote]) - fetch + merge into current branch
JSValue js_git_pull(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *remote_name = "origin";
    const char *remote_js = NULL;

    if (argc >= 1 && JS_IsString(argv[0])) {
        remote_js = JS_ToCString(ctx, argv[0]);
        if (!remote_js)
            return JS_ThrowInternalError(ctx, "Invalid remote name");
        remote_name = remote_js;
    }

    git_libgit2_init();

    git_repository *repo = NULL;
    git_remote *remote = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_remote_lookup(&remote, repo, remote_name);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error looking up remote", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    fetch_opts.callbacks.credentials = credentials_cb;
    fetch_opts.callbacks.certificate_check = certificate_check_cb;
    fetch_opts.callbacks.transfer_progress = transfer_progress_cb;

    printf("Pull: fetching from '%s'...\n", remote_name);
    err = git_remote_fetch(remote, NULL, &fetch_opts, NULL);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error fetching from remote", err);
        git_remote_free(remote);
        git_repository_free(repo);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    char branch[256];
    err = get_current_branch_name(repo, branch, sizeof(branch));
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error reading current branch", err);
        git_remote_free(remote);
        git_repository_free(repo);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    char remote_ref[512];
    snprintf(remote_ref, sizeof(remote_ref), "refs/remotes/%s/%s", remote_name, branch);

    git_annotated_commit *their_head = NULL;
    err = git_annotated_commit_from_revspec(&their_head, repo, remote_ref);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error resolving remote branch", err);
        git_remote_free(remote);
        git_repository_free(repo);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    const git_annotated_commit *heads[] = { their_head };
    err = git_merge(repo, heads, 1, NULL, NULL);

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error merging fetched commit", err);
    else
        result = JS_NewString(ctx, JS_SUPPRESS);

    git_annotated_commit_free(their_head);
    git_remote_free(remote);
    git_repository_free(repo);
    if (remote_js) JS_FreeCString(ctx, remote_js);
    git_libgit2_shutdown();
    return result;
}

// git.push([remote]) - push current branch to remote
JSValue js_git_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *remote_name = "origin";
    const char *remote_js = NULL;

    if (argc >= 1 && JS_IsString(argv[0])) {
        remote_js = JS_ToCString(ctx, argv[0]);
        if (!remote_js)
            return JS_ThrowInternalError(ctx, "Invalid remote name");
        remote_name = remote_js;
    }

    git_libgit2_init();

    git_repository *repo = NULL;
    git_remote *remote = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_remote_lookup(&remote, repo, remote_name);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error looking up remote", err);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    char branch[256];
    err = get_current_branch_name(repo, branch, sizeof(branch));
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error reading current branch", err);
        git_remote_free(remote);
        git_repository_free(repo);
        if (remote_js) JS_FreeCString(ctx, remote_js);
        git_libgit2_shutdown();
        return ret;
    }

    char spec_buf[600];
    snprintf(spec_buf, sizeof(spec_buf), "refs/heads/%s:refs/heads/%s", branch, branch);
    char *specs[1];
    specs[0] = spec_buf;
    git_strarray refspecs = { specs, 1 };

    git_push_options push_opts = GIT_PUSH_OPTIONS_INIT;
    push_opts.callbacks.credentials = credentials_cb;
    push_opts.callbacks.certificate_check = certificate_check_cb;
    push_opts.callbacks.transfer_progress = transfer_progress_cb;

    printf("Pushing '%s' to '%s'...\n", branch, remote_name);
    err = git_remote_push(remote, &refspecs, &push_opts);

    JSValue result;
    if (err < 0)
        result = js_git_error(ctx, "Error pushing to remote", err);
    else
        result = JS_NewString(ctx, JS_SUPPRESS);

    git_remote_free(remote);
    git_repository_free(repo);
    if (remote_js) JS_FreeCString(ctx, remote_js);
    git_libgit2_shutdown();
    return result;
}

// git.log([n]) - print last n commits (default 10)
JSValue js_git_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t max_count = 10;
    if (argc >= 1 && JS_IsNumber(argv[0])) {
        JS_ToInt32(ctx, &max_count, argv[0]);
        if (max_count <= 0)
            max_count = 10;
    }

    git_libgit2_init();

    git_repository *repo = NULL;
    git_revwalk *walk = NULL;
    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_revwalk_new(&walk, repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error creating revwalk", err);
        git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    git_revwalk_sorting(walk, GIT_SORT_TIME);
    git_revwalk_push_head(walk);

    git_oid oid;
    int count = 0;
    while (count < max_count && git_revwalk_next(&oid, walk) == 0) {
        git_commit *commit = NULL;
        if (git_commit_lookup(&commit, repo, &oid) != 0)
            continue;

        char oidstr[9];
        git_oid_tostr(oidstr, sizeof(oidstr), &oid);

        const git_signature *author = git_commit_author(commit);
        const char *summary = git_commit_summary(commit);

        printf("%s %s <%s> %s\n",
               oidstr,
               author && author->name ? author->name : "?",
               author && author->email ? author->email : "?",
               summary ? summary : "");

        git_commit_free(commit);
        count++;
    }

    git_revwalk_free(walk);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return JS_NewString(ctx, JS_SUPPRESS);
}

// git.stash([message]) - stash current changes
JSValue js_git_stash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *msg = NULL;
    const char *msg_js = NULL;

    if (argc >= 1 && JS_IsString(argv[0])) {
        msg_js = JS_ToCString(ctx, argv[0]);
        if (!msg_js)
            return JS_ThrowInternalError(ctx, "Invalid message");
        msg = msg_js;
    }

    git_libgit2_init();

    git_repository *repo = NULL;
    git_signature *sig = NULL;
    git_oid oid;

    int err = open_repo_here(&repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error opening repository", err);
        if (msg_js) JS_FreeCString(ctx, msg_js);
        git_libgit2_shutdown();
        return ret;
    }

    err = make_signature(&sig, repo);
    if (err < 0) {
        JSValue ret = js_git_error(ctx, "Error creating signature", err);
        git_repository_free(repo);
        if (msg_js) JS_FreeCString(ctx, msg_js);
        git_libgit2_shutdown();
        return ret;
    }

    err = git_stash_save(&oid, repo, sig, msg, GIT_STASH_DEFAULT);

    JSValue result;
    if (err == GIT_ENOTFOUND) {
        printf("No local changes to stash.\n");
        result = JS_NewString(ctx, JS_SUPPRESS);
    } else if (err < 0) {
        result = js_git_error(ctx, "Error creating stash", err);
    } else {
        result = JS_NewString(ctx, JS_SUPPRESS);
    }

    git_signature_free(sig);
    git_repository_free(repo);
    if (msg_js) JS_FreeCString(ctx, msg_js);
    git_libgit2_shutdown();
    return result;
}