#ifndef CCM_H
#define CCM_H

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"


#ifndef CCM_OUTPUT_OPT
#define CCM_OUTPUT_OPT "-o"
#endif /* CCM_OUTPUT_OPT */

#ifndef CCM_INITIAL_CAP
#define CCM_INITIAL_CAP 32
#endif /* CCM_INITIAL_CAP */

#define countof(a) ((lll)(sizeof((a))/sizeof((a)[0])))

#define ccm_string_array_len(...) countof(((c8 *[]){__VA_ARGS__}))
#define ccm_string_array(...)                           \
    (ccm_string_array) {                                \
        .length = ccm_string_array_len(__VA_ARGS__),    \
            .items = (c8 *[]){ __VA_ARGS__ },           \
            }

#define ccm_target_array_len(...) countof(((ccm_target *[]){__VA_ARGS__}))
#define ccm_target_array(...)                           \
    (ccm_target_array) {                                \
        .length = ccm_target_array_len(__VA_ARGS__),    \
            .items = (ccm_target *[]){ __VA_ARGS__ },   \
            }

#define ccm_deps_array_len(...) ccm_target_array_len(__VA_ARGS__)
#define ccm_deps_array(...)     ccm_target_array(__VA_ARGS__)

#define ccm_da_append(da, item)                                         \
    do {                                                                \
        lll cap = (da)->capacity;                                       \
        lll len = (da)->length;                                         \
        lll item_size = sizeof((da)->items[0]);                         \
        if (cap == len) {                                               \
            cap = cap == 0? CCM_INITIAL_CAP : cap * 2;                  \
            void *p = realloc((da)->items, (uword)(cap * item_size));   \
            if (p == NULL) {                                            \
                ccm_log(CCM_ERROR,                                      \
                        "realloc failed at %s: %d",                     \
                        __FILE_NAME__, __LINE__);                       \
            }                                                           \
            (da)->items = p;                                            \
            (da)->capacity = cap;                                       \
        }                                                               \
        (da)->items[len] = item;                                        \
        ++(da)->length;                                                 \
    } while(0)

#define ccm_da_free(da)                         \
    do {                                        \
        (da)->capacity = 0;                     \
        (da)->length = 0;                       \
        free((da)->items);                      \
        (da)->items = 0;                        \
    } while(0)

#define ccm_unreachable(msg) assert(false && msg)
#define ccm_unused(v) ((void)v)

typedef enum {
    CCM_INFO,
    CCM_WARN,
    CCM_DEBUG,
    CCM_TRACE,
    CCM_ERROR,
    CCM_LOG_LAST,
} ccm_log_level;

typedef struct {
    lll length;
    c8 **items;
} ccm_string_array;

typedef struct {
    lll capacity;
    lll length;
    c8 const**items;
} ccm_string_da;

typedef struct ccm_target ccm_target;
typedef struct {
    lll length;
    ccm_target **items;
} ccm_target_array;

struct ccm_target {
    c8 *name;
    ccm_string_array sources;
    ccm_string_array pre_opts;
    ccm_string_array post_opts;
    ccm_target_array deps;
    i32 visited;
    i32 collected;
    i32 j;
};

typedef struct {
    c8 const *compiler;
    ccm_string_array common_opts;
    ccm_target_array targets;
    ccm_string_da sb;
} ccm_spec;

c8* ccm_shift_args(i32 *argc, c8 ***argv);
void ccm_stats();
void ccm_log(ccm_log_level l, c8 const *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

time_t ccm_mtime(void);

void ccm_sb_print(ccm_log_level l, ccm_string_da sb);
void ccm_sb_exec(ccm_string_da sb);

bool ccm_target_needs_rebuild(ccm_target *t);

void ccm_spec_restore_default_options(ccm_spec *b);
void ccm_spec_prepare_common_prefix(ccm_spec *b);
void ccm_spec_build_target(ccm_spec *b, ccm_target *t);
void ccm_spec_build(ccm_spec *b);
void ccm_spec_clean(ccm_spec *b);

void ccm_bootstrap(c8 const* compiler, i32 argc, c8 **argv);

#endif /* CCM_H */

#ifdef CCM_IMPLEMENTATION

#ifdef CCM_STATS
static i32 fork_count = 0;
static i32 exec_count = 0;
static i32 stat_count = 0;
#define fork() (++fork_count, fork())
#define exec(...) (++exec_count, exec(__VA_ARGS__))
#define stat(path, buf) (++stat_count, stat(path, buf))
#endif /* CCM_STATS */

c8* ccm_shift_args(i32 *argc, c8 ***argv)
{
    c8 *r = (*argv)[0];
    --(*argc);
    ++(*argv);
    return r;
}

#ifdef CCM_STATS
void ccm_stats()
{
    ccm_log(CCM_TRACE, "CCM_STATS: fork_count = %d\n", fork_count);
    ccm_log(CCM_TRACE, "CCM_STATS: exec_count = %d\n", exec_count);
    ccm_log(CCM_TRACE, "CCM_STATS: stat_count = %d\n", stat_count);
}
#else
#define ccm_stats()
#endif /* CCM_STATS */

void ccm_log(ccm_log_level l, c8 const *fmt, ...)
{
    c8 const *level = NULL;
    va_list ap;
    switch(l) {
    case CCM_INFO:  level = "[INFO]";  break;
    case CCM_WARN:  level = "[WARN]";  break;
    case CCM_DEBUG: level = "[DEBUG]"; break;
    case CCM_TRACE: level = "[TRACE]"; break;
    case CCM_ERROR: level = "[ERROR]"; break;
    default:        level = "[UNKNOWN]"; break;
    }

    fprintf(stderr, "%-10s ", level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

time_t ccm_mtime(void)
{
    static struct stat ccm_h;
    static struct stat ccm_c;
    if (!ccm_h.st_mtime) stat("./ccm.h", &ccm_h);
    if (!ccm_c.st_mtime) stat("./ccm.c", &ccm_c);
    return ccm_h.st_mtime < ccm_c.st_mtime ? ccm_c.st_mtime : ccm_h.st_mtime;
}

void ccm_sb_print(ccm_log_level l, ccm_string_da sb)
{
    i32 len = 0;
    for (i32 i = 0; i < sb.length - 1; ++i) len += strlen(sb.items[i]) + 1;
    c8 *buf = malloc(len + 1);
    if (buf == NULL) {
        ccm_log(CCM_ERROR, "CMD: malloc failed\n");
        return;
    }

    i32 remaining = len + 1;
    c8 *itr_buf = buf;
    for (i32 i = 0; i < sb.length - 1; ++i) {
        i32 n = snprintf(itr_buf, (uword)remaining, "%s ", sb.items[i]);
        if (n < 0 || n >= remaining) {
            ccm_log(CCM_ERROR, "CMD: possible buffer overflow\n");
            free(buf);
            return;
        }
        itr_buf += n;
        remaining -= n;
    }
    ccm_log(l, "CMD: %s\n", buf);
    free(buf);
}

void ccm_sb_exec(ccm_string_da sb)
{
    ccm_sb_print(CCM_INFO, sb);
    c8 const *pathname = sb.items[0];
    c8 const** argv = &sb.items[0];
    pid_t pid = fork();
    switch(pid) {
    case 0: {
        execvp(pathname, (c8 *const*)argv);
        ccm_log(CCM_ERROR, "execvp failed!\n");
        exit(69);
        break;
    }
    default:
        /* printf("Main process is building %s\n", b->sb.items[b->sb.length - 2]); */
    }
}

bool ccm_target_needs_rebuild(ccm_target *t)
{
    struct stat outfile_stat;
    struct stat srcfile_stat;

    time_t output_mtime = 0;

    if (stat(t->name, &outfile_stat) != -1) {
        output_mtime = outfile_stat.st_mtime;
    }
    if (output_mtime < ccm_mtime()) return true;

    for (i32 i = 0; i < t->sources.length; ++i) {
        if (stat(t->sources.items[i], &srcfile_stat) != -1 &&
            output_mtime < srcfile_stat.st_mtime) {
            return true;
        }
        /* TODO: is this really needed? */
        /* else  { */
        /*     ccm_log(CCM_ERROR, "Target: %s build failed, missing %s source file", */
        /*             t->name, t->sources.items[i]); */
        /*     return false; */
        /* } */
    }
    return false;
}

void ccm_spec_restore_common_opts(ccm_spec *b)
{
    b->sb.length = 1 + b->common_opts.length;
}

void ccm_spec_prepare_common_prefix(ccm_spec *b)
{
    ccm_da_append(&b->sb, b->compiler);
    for (i32 i = 0; i < b->common_opts.length; ++i) {
        ccm_da_append(&b->sb, b->common_opts.items[i]);
    }
}

void ccm_spec_build_target(ccm_spec *b, ccm_target *t)
{
    if (t->visited) {
        ccm_log(CCM_ERROR, "Target: %s is part of a cycle\n", t->name);
        return;
    }
    t->visited = 1;
    if (t->collected) {
        ccm_log(CCM_TRACE, "Target: %s is already built\n", t->name);
        return;
    }
    ccm_log(CCM_TRACE, "Target: %s is being built...\n", t->name ? t-> name :".phony");
    for (i32 i = 0; i < t->deps.length; ++i) {
        ccm_spec_build_target(b, t->deps.items[i]);
    }

    if (!ccm_target_needs_rebuild(t)) {
        ccm_log(CCM_INFO, "Target: %s is up to date\n", t->name);
        return;
    }

    for (i32 i = 0; i < t->pre_opts.length; ++i) {
        ccm_da_append(&b->sb, t->pre_opts.items[i]);
    }

    if (t->name != NULL) {
        /* if there is an output for target t,
         * add -o <name> to the build command */
        ccm_da_append(&b->sb, CCM_OUTPUT_OPT);
        ccm_da_append(&b->sb, t->name);
    }

    for (i32 i = 0; i < t->sources.length; ++i) {
        ccm_da_append(&b->sb, t->sources.items[i]);
    }

    for (i32 i = 0; i < t->post_opts.length; ++i) {
        ccm_da_append(&b->sb, t->post_opts.items[i]);
    }

    ccm_da_append(&b->sb, NULL);
    ccm_sb_exec(b->sb); t->collected = 1;
    ccm_spec_restore_common_opts(b);
}

void ccm_spec_build(ccm_spec *b)
{
    ccm_spec_prepare_common_prefix(b);
    for (i32 i = 0; i < b->targets.length; ++i) {
        /* TODOs:
        * 1-fork here:
        *    I think that's the correct way to do it for parallelaizable
        *    builds with correct dependency handling
        * 2-Handle communication with child processes and status codes.
        */
        ccm_spec_build_target(b, b->targets.items[i]);
    }
    ccm_da_free(&b->sb);
    i32 status;
    while (wait(&status) != -1) ccm_log(CCM_DEBUG, "status = %d\n", status);
    ccm_stats();
}

void ccm_spec_clean(ccm_spec *b)
{
    for (i32 i = 0; i < b->targets.length; ++i) {
        if (unlink(b->targets.items[i]->name) == -1) {
            ccm_log(CCM_ERROR, "rm %s failed!\n", b->targets.items[i]->name);
            continue;
        }
        b->targets.items[i]->visited = 0;
        b->targets.items[i]->collected = 0;
    }
}

void ccm_bootstrap(c8 const* compiler, i32 argc, c8 **argv)
{
    ccm_unused(argc);
    ccm_spec b = {
        .compiler = compiler,
        .common_opts =
        ccm_string_array("-Wall", "-Wextra", "-g3", "-O2", "-fsanitize=address,bounds",
                         "-I."),
    };
    ccm_target self = {
        .name = "./ccm",
        .sources = ccm_string_array("./ccm.c"),
    };
    struct stat outfile_stat;
    time_t output_mtime = 0;

    if (stat(self.name, &outfile_stat) != -1) {
        output_mtime = outfile_stat.st_mtime;
    } else {
        ccm_unreachable("unreachable: build_bootstrap");
    }
    /* label */
    if (ccm_mtime() < output_mtime) {
        ccm_log(CCM_TRACE, "ccm up to date, skip bootstrapping\n");
     return;
    }
    ccm_log(CCM_TRACE, "ccm bootstrapping ...\n");
    rename("./ccm", "./ccm.old");
    b.targets = ccm_target_array(&self);
    /* label */
    ccm_spec_build(&b);

    if (stat(self.name, &outfile_stat) != -1) {
        unlink("./ccm.old");
        execvp("./ccm", argv);
    } else {
        ccm_log(CCM_ERROR, "ccm bootstrapping failed\n");
        rename("./ccm.old", "./ccm");
    }

}
#endif /* CCM_IMPLEMENTATION */

/* TODOs
 * Correct bootstraping stat-syscall handling:
 *   in ccm_bootstrap, "./ccm" executable is checked twice at the label points
 * Correct Handling of deps with respect to forking:
 *   forcing targets-with-deps to build in correct order.
 */

/* DONEs
 * Logging:
 *   improve logging with log levels.
 * Caching:
 *   use stat to check if target->name is older than any of the sources/deps.
 * Bootstrapping:
     allow ccm to build itself
 * Naming: (iteration 1)
 *   more consistent naming, better name criteria
 * Refactor:
 *   refactoring into per-target functions
 */
