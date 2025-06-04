#ifndef CCM_H
#define CCM_H

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

// BEGIN Types {
typedef int8_t   s8 ;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint8_t  u8 ;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef char c8;
typedef unsigned char uc8;

typedef size_t    uw;
typedef ssize_t   sw;
typedef ptrdiff_t lll;

typedef struct ccm_target ccm_target;
// } END Types

// BEGIN Macro {
#ifdef __GNUC__
#    define ccm_unreachable()  __builtin_unreachable()
#elif defined(_MSC_VER)
#    define ccm_unreachable()  __assume(false)
#else
#    define ccm_unreachable()
#endif
#define ccm_assert(expr)     assert(expr)
#define ccm_unused(v)        ((void)(v))
#define ccm_ignore(v)        ccm_unused(v)
#define ccm_countof(a)       ((lll)(sizeof((a))/sizeof((a)[0])))
#define ccm_lenof(a)      (ccm_countof(a) - 1)
#define ccm_container_of(ptr, type, member) /* https://en.wikipedia.org/wiki/Offsetof */ \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#ifdef __GNUC__
#    define CCM_ATTR_PRINTF(fmt, args) __attribute__ ((format (printf, fmt, args)))
#else
#    define CCM_ATTR_PRINTF(fmt, args)
#endif

#define ccm_swap(T, x, y)                       \
    do {                                        \
        T *xx = &(x);                           \
        T *yy = &(y);                           \
        T temp = *xx;                           \
        *xx = *yy;                              \
        *yy = temp;                             \
    } while (0)

#ifndef ccm_api
#define ccm_api static inline
#endif /* CCMAPI */

#define ccm_internal static inline

#define ccm_string_array_len(...) ccm_countof(((c8 *[]){__VA_ARGS__}))
#define ccm_string_array(...)                           \
    (ccm_string_array) {                                \
        .len = ccm_string_array_len(__VA_ARGS__),       \
            .items = (c8 *[]){ __VA_ARGS__ },           \
            }

#define X__deps_array_len(...) ccm_countof(((ccm_target *[]){__VA_ARGS__}))
#define X__deps_array(...)                              \
    (ccm_target_array) {                                \
        .len = X__deps_array_len(__VA_ARGS__),          \
            .items = (ccm_target *[]){ __VA_ARGS__ },   \
            }

#define ccm_deps_array_len(...) X__deps_array_len(&ccm_bootstrap_target, __VA_ARGS__)
#define ccm_deps_array(...)     X__deps_array(&ccm_bootstrap_target, __VA_ARGS__)
// } END Macros

#include "log.h"
#include "arena.h"
#include "rb.h"


// BEGIN Dynamic Array Decl {
#define CCM_DA_INITIAL_CAP 4

#define ccm_da_for(i, da)        for (i = 0, i < (da)->len; ++i)

#define X__da_foreach_it(t, da) X__da_foreach_itrname(t, it, da)
#define X__da_foreach_itrname(t, itrname, da)   \
    for (t *itrname = (da)->items;              \
         itrname != (da)->items + (da)->len;    \
         ++itrname)

#define X__da_foreach_x(_1, _2, _3, name, ...) name
#define ccm_da_foreach(...)                                             \
    X__da_foreach_x(__VA_ARGS__,                                        \
                    X__da_foreach_itrname,                              \
                    X__da_foreach_it, /* dummy */ )(__VA_ARGS__)

#define ccm_da_append(da, item)                                         \
    do {                                                                \
        lll cap = (da)->cap;                                            \
        lll len = (da)->len;                                            \
        lll item_size = sizeof((da)->items[0]);                         \
        if (cap == len) {                                               \
            cap = cap == 0? CCM_DA_INITIAL_CAP : cap * 2;               \
            void *p = ccm_realloc((da)->items, (cap * item_size));      \
            if (p == NULL) {                                            \
                ccm_log(CCM_ERROR,                                      \
                        "realloc failed at %s: %d",                     \
                        __FILE_NAME__, __LINE__);                       \
            }                                                           \
            (da)->items = p;                                            \
            (da)->cap = cap;                                            \
        }                                                               \
        (da)->items[len] = item;                                        \
        ++(da)->len;                                                    \
    } while(0)

#define ccm_da_deinit(da)                       \
    do {                                        \
        ccm_assert((da)->items);                \
        ccm_free((da)->items);                  \
        (da)->cap = 0;                          \
        (da)->len = 0;                          \
        (da)->items = NULL;                     \
    } while (0)
// } END Dynamic Array Decl

#ifndef CCM_OUTPUT_OPT
#define CCM_OUTPUT_OPT "-o"
#endif /* CCM_OUTPUT_OPT */

#ifndef CCM_INITIAL_CAP
#define CCM_INITIAL_CAP 32
#endif /* CCM_INITIAL_CAP */


typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef ptrdiff_t lll;

typedef char c8;

typedef struct {
    lll len;
    c8 **items;
} ccm_string_array;

typedef struct {
    lll cap;
    lll len;
    c8 const**items;
} ccm_str_da;

typedef struct ccm_target ccm_target;
typedef struct {
    lll len;
    ccm_target **items;
} ccm_target_array;

struct ccm_target {
    c8 *name;
    ccm_string_array sources;
    ccm_string_array pre_opts;
    ccm_string_array post_opts;
    ccm_target_array deps;

    pid_t pid;
    s32 visited;
    s32 collected;
    s32 status;
};

typedef struct {
    c8 const *compiler;
    ccm_string_array common_opts;
    ccm_target_array deps;
    ccm_str_da sb;
    c8 **argv;
    s32 argc;
    s32 j;
    ccm_ring_buffer schedule;
} ccm_spec;

c8* ccm_shift_args(s32 *argc, c8 ***argv);
void ccm_stats();

time_t ccm_mtime(void);

void ccm_sb_print(ccm_log_level l, ccm_str_da sb);
pid_t ccm_sb_exec(ccm_str_da sb);

bool ccm_target_needs_rebuild(ccm_target *t);

void ccm_spec_restore_default_options(ccm_spec *b);
void ccm_spec_prepare_common_prefix(ccm_spec *b);
void ccm_spec_build_target(ccm_spec *b, ccm_target *t);
void ccm_spec_build(ccm_spec *b);
void ccm_spec_clean(ccm_spec *b);

static ccm_target ccm_bootstrap_target = {
    .name = "./ccm",
    .sources = ccm_string_array("ccm.h", "ccm.c"),
};

#endif /* CCM_H */

/* #ifdef CCM_IMPLEMENTATION */
#if 1

#ifdef CCM_STATS
static s32 fork_count = 0;
static s32 exec_count = 0;
static s32 stat_count = 0;
#define fork() (++fork_count, vfork())
#define exec(...) (++exec_count, exec(__VA_ARGS__))
#define stat(path, buf) (++stat_count, stat(path, buf))
#endif /* CCM_STATS */

c8* ccm_shift_args(s32 *argc, c8 ***argv)
{
    c8 *r = (*argv)[0];
    --(*argc);
    ++(*argv);
    return r;
}

#ifdef CCM_STATS
void ccm_stats()
{
    ccm_log(CCM_DEBUG, "CCM_STATS: fork_count = %d\n", fork_count);
    ccm_log(CCM_DEBUG, "CCM_STATS: exec_count = %d\n", exec_count);
    ccm_log(CCM_DEBUG, "CCM_STATS: stat_count = %d\n", stat_count);
}
#else
#define ccm_stats()
#endif /* CCM_STATS */

void ccm_sb_print(ccm_log_level l, ccm_str_da sb)
{
    s32 len = 0;
    for (s32 i = 0; i < sb.len - 1; ++i) len += strlen(sb.items[i]) + 1;
    c8 *buf = malloc(len + 1);
    if (buf == NULL) {
        ccm_log(CCM_ERROR, "CMD: malloc failed\n");
        return;
    }

    s32 remaining = len + 1;
    c8 *itr_buf = buf;
    for (s32 i = 0; i < sb.len - 1; ++i) {
        s32 n = snprintf(itr_buf, remaining, "%s ", sb.items[i]);
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

pid_t ccm_sb_exec(ccm_str_da sb)
{
    ccm_sb_print(CCM_INFO, sb);
    c8 const *pathname = sb.items[0];
    c8 const** argv = &sb.items[0];
    pid_t pid = fork();
    switch(pid) {
    case -1: {
        ccm_log(CCM_ERROR, "vfork failed\n");
        exit(1);
    }
    case 0: {
        if (execvp(pathname, (c8 *const*)argv) < 0) {
            ccm_log(CCM_ERROR, "execvp failed!\n");
            _exit(69);
        }
        ccm_unreachable();
        break;
    }
    default:
        return pid;
        /* printf("Main process is building %s\n", b->sb.items[b->sb.len - 2]); */
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
    if (output_mtime == 0) return true;

    for (s32 i = 0; i < t->sources.len; ++i) {
        if (stat(t->sources.items[i], &srcfile_stat) != -1 &&
            output_mtime < srcfile_stat.st_mtime) {
            return true;
        }
    }
    return false;
}

void ccm_spec_restore_common_opts(ccm_spec *b)
{
    b->sb.len = 1 + b->common_opts.len;
}

void ccm_spec_prepare_common_prefix(ccm_spec *b)
{
    b->sb.len = 0;
    ccm_da_append(&b->sb, b->compiler);
    for (s32 i = 0; i < b->common_opts.len; ++i) {
        ccm_da_append(&b->sb, b->common_opts.items[i]);
    }
}

void ccm_spec_schedule(ccm_spec *s)
{
    for (s32 i = 0; i < s->deps.len; ++i) {
        for (s32 j = 0; j < s->deps.items[i]->deps.len; ++i) {
        }
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
        ccm_log(CCM_INFO, "Target: %s is already built\n", t->name);
        return;
    }
    for (s32 i = 0; i < t->deps.len; ++i) {
        ccm_spec_build_target(b, t->deps.items[i]);
    }
    if (!ccm_target_needs_rebuild(t)) {
        ccm_log(CCM_INFO, "Target: %s is up to date\n", t->name);
        return;
    }

    ccm_log(CCM_INFO, "Target: %s building in progress...\n",
            t->name ? t-> name :".phony");

    for (s32 i = 0; i < t->pre_opts.len; ++i) {
        ccm_da_append(&b->sb, t->pre_opts.items[i]);
    }

    for (s32 i = 0; i < t->sources.len; ++i) {
        ccm_da_append(&b->sb, t->sources.items[i]);
    }

    if (t->name != NULL) {
        /* if there is an output for target t,
         * add -o <name> to the build command */
        ccm_da_append(&b->sb, CCM_OUTPUT_OPT);
        ccm_da_append(&b->sb, t->name);
    }

    for (s32 i = 0; i < t->post_opts.len; ++i) {
        ccm_da_append(&b->sb, t->post_opts.items[i]);
    }

    ccm_da_append(&b->sb, NULL);
    t->pid = ccm_sb_exec(b->sb); t->collected = 1;
    ccm_spec_restore_common_opts(b);
}

void ccm_spec_build(ccm_spec *b)
{
    ccm_spec_prepare_common_prefix(b);

    if (ccm_target_needs_rebuild(b->deps.items[0])) {
        rename(b->deps.items[0]->name, "ccm.old");
        ccm_spec_build_target(b, b->deps.items[0]);
        waitpid(b->deps.items[0]->pid, &b->deps.items[0]->status, 0);

        if (b->deps.items[0]->status) {
            ccm_log(CCM_ERROR, "bootstrap failed\n");
            rename("ccm.old", b->deps.items[0]->name);
            exit(1);
        } else {
            ccm_log(CCM_INFO, "bootstrap succeeded\n");
            execvp(b->deps.items[0]->name, b->argv);
            ccm_log(CCM_ERROR, "execvp failed: %s\n", strerror(errno));
            exit(1);
        }
    }
    for (s32 i = 1; i < b->deps.len; ++i) {
        /* TODOs:
         * 1-Handle communication with child processes and status codes.
         */
        ccm_spec_build_target(b, b->deps.items[i]);
    }
    ccm_da_deinit(&b->sb);

    ccm_stats();

    /* for (s32 i = 1; i < b->targets.len; ++i) { */
    /*     if (b->targets.items[i]->pid) { */
    /*         waitpid(b->targets.items[i]->pid, &b->targets.items[i]->status, 0); */
    /*         if (b->targets.items[i]->status) { */
    /*             ccm_log(CCM_ERROR, "Target: %s failed to build, exit status = %d\n", */
    /*                     b->targets.items[i]->name, */
    /*                     b->targets.items[i]->status); */
    /*         } */
    /*     } */
    /* } */
}

void ccm_spec_clean(ccm_spec *b)
{
    for (s32 i = 1; i < b->deps.len; ++i) {
        if (remove(b->deps.items[i]->name) == -1) {
            ccm_log(CCM_ERROR, "rm %s failed!\n", b->deps.items[i]->name);
            continue;
        }
        b->deps.items[i]->visited = 0;
        b->deps.items[i]->collected = 0;
    }
}
#endif /* CCM_IMPLEMENTATION */

/* TODOs
 * Redirection:
 *   Proper output redirection with pipes to ensure deterministic output order
 * Correct Handling of deps with respect to forking:
 *   forcing targets-with-deps to build in correct order.
 */

/* That's ok, leave it for now
 * Correct bootstrapping stat-syscall handling:
 *   in ccm_bootstrap, "./ccm" executable is checked twice at the label points
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
