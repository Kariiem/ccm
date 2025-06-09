#ifndef CCM_H
#define CCM_H

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
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

// -----------------------------------------------------------------------------
// [1] Aliases
// -----------------------------------------------------------------------------
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

typedef struct pollfd pollfd;
// -----------------------------------------------------------------------------
// [2] Config & Helper Macros
// -----------------------------------------------------------------------------
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
#define ccm_lengthof(a)      (ccm_countof(a) - 1)
#define ccm_container_of(ptr, type, member) /* https://en.wikipedia.org/wiki/Offsetof */ \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#define ccm_swap(T, x, y)                       \
    do {                                        \
        T *xx = &(x);                           \
        T *yy = &(y);                           \
        T temp = *xx;                           \
        *xx = *yy;                              \
        *yy = temp;                             \
    } while (0)

#ifndef ccm_malloc
#define ccm_malloc malloc
#endif /* backend_malloc */

#ifndef ccm_calloc
#define ccm_calloc(cap) calloc(1, cap)
#endif /* backend_malloc */

#ifndef ccm_realloc
#define ccm_realloc realloc
#endif /* backend_realloc */

#ifndef ccm_reallocarray
#define ccm_reallocarray reallocarray
#endif /* base_reallocarray */

#ifndef ccm_free
#define ccm_free free
#endif  /* ccm_free */

#ifdef __GNUC__
#    define CCM_ATTR_PRINTF(fmt, args) __attribute__ ((format (printf, fmt, args)))
#else
#    define CCM_ATTR_PRINTF(fmt, args)
#endif

#define ccm_internal static

#ifndef ccm_api
#define ccm_api static
#endif

// -----------------------------------------------------------------------------
// [3] Logger
// -----------------------------------------------------------------------------
typedef enum {
    CCM_LOG_INFO,
    CCM_LOG_WARN,
    CCM_LOG_DEBUG,
    CCM_LOG_ERROR,
    CCM_LOG_NONE,
    CCM_LOG_LAST,
} ccm_log_level;

void ccm_panic(c8 const *fmt, ...)  CCM_ATTR_PRINTF(1, 2);
void ccm_log(ccm_log_level l, c8 const *fmt, ...)  CCM_ATTR_PRINTF(2, 3);

// -----------------------------------------------------------------------------
// [4] Arena
// -----------------------------------------------------------------------------
#ifndef CCM_ARENA_DEFAULT_CAP
#define CCM_ARENA_DEFAULT_CAP (64 * 1024 * 1024) /* 64mb */
#endif /* CCM_ARENA_DEFAULT_CAP */

typedef struct {
    u8 *memory;
    lll capacity;
    lll offset;
} ccm_arena;

ccm_arena ccm_arena_init(lll capacity);
void *ccm_arena_allocarray(ccm_arena *arena, lll itemsize, lll itemcount, lll align);
void  ccm_arena_reset(ccm_arena *arena);
void  ccm_arena_log_stats(ccm_arena *arena);
void  ccm_arena_deinit(ccm_arena *arena);

#define ccm_lifetime(arena) as_scratch_arena(arena)
#define ccm_as_scratch_arena(arena)                             \
    for (ccm_arena _restore_arena = (arena),                    \
             *_arena_ptr = &(arena);                            \
         _arena_ptr != NULL;                                    \
         *_arena_ptr = _restore_arena, _arena_ptr = NULL)

#define ccm_arena_type_align(t,a)   (t *)arena_allocarray(a, 0, 0, alignof(t))

#define X__arena_alloc_x(_1, _2, _3, name, ...) name
#define X__arena_alloc_1(t, a    ) (t *)ccm_arena_allocarray(a, sizeof(t), 1, alignof(t))
#define X__arena_alloc_n(t, a, c ) (t *)ccm_arena_allocarray(a, sizeof(t), c, alignof(t))
#define ccm_arena_alloc(...)                                    \
    X__arena_alloc_x(__VA_ARGS__,                               \
                     X__arena_alloc_n,                          \
                     X__arena_alloc_1, /*dummy*/)(__VA_ARGS__)

#define ccm_arena_flexalloc(t, m, a, c)                                 \
    (t*)ccm_arena_allocarray(a, offsetof(t, m[c]), 1, alignof(t))

// -----------------------------------------------------------------------------
// [5] Ring Buffer
// -----------------------------------------------------------------------------
#define CCM_RINGBUFFER_INITIAL_CAP 64

typedef struct ccm_target ccm_target;
typedef ccm_target* ccm_rbvalue_t;
typedef struct {
    s32 read;
    s32 write;
    s32 cap;
    s32 len;
    ccm_rbvalue_t *items;
} ccm_ring_buffer;

ccm_ring_buffer ccm_rb_init(s32 cap);
void            ccm_rb_deinit(ccm_ring_buffer *rb);

void          ccm_rb_push(ccm_ring_buffer *rb, ccm_rbvalue_t v);
ccm_rbvalue_t ccm_rb_pop(ccm_ring_buffer *rb);
ccm_rbvalue_t ccm_rb_peek(ccm_ring_buffer const *rb);

// -----------------------------------------------------------------------------
// [6] Dynamic Array
// -----------------------------------------------------------------------------
#define CCM_DA_INITIAL_CAP 16

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
                ccm_log(CCM_LOG_ERROR,                                  \
                        "ccm_realloc failed at %s: %d",                 \
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

// -----------------------------------------------------------------------------
// [7] Core
// -----------------------------------------------------------------------------
#ifndef CCM_CHILD_PROC_INITIAL_BUFFER_CAP
#define CCM_CHILD_PROC_INITIAL_BUFFER_CAP (8*1024) /* 8kb */
#endif /* CCM_CHILD_PROC_OUTPUT_BUFFER_CAP */


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

#define ccm_deps_array_len(...) X__deps_array_len(__VA_ARGS__)
#define ccm_deps_array(...)     X__deps_array(__VA_ARGS__)

typedef struct {
    lll cap;
    lll len;
    c8 *items;
} ccm_str_builder;

typedef struct {
    lll len;
    c8 **items;
} ccm_string_array;

typedef struct {
    lll cap;
    lll len;
    c8 const**items;
} ccm_str_da;

typedef struct {
    lll len;
    ccm_target **items;
} ccm_target_array;

typedef struct pollfd ccm_pollfd;

typedef struct {
    s32 read;
    s32 write;
} ccm_pipe;

typedef struct {
    pid_t pid;
    s32 id;
    s32 status;
    ccm_pipe pipe;
    c8 **cmd;
    c8 *report;
    clock_t time;
    ccm_target const *target;
} ccm_child_proc;

struct ccm_target {
    c8 *name;
    ccm_string_array sources;
    ccm_string_array compiler_opts;
    ccm_string_array linker_opts;
    ccm_target_array deps;
    ccm_target_array revdeps;

    s32 level;
    s32 visited;
    s32 collected;
};

typedef struct {
    c8 *compiler;
    c8 *output_flag;
    ccm_string_array common_opts;
    ccm_target_array deps;
    ccm_arena arena;
    c8 **argv;
    s32 argc;
    s32 j;
} ccm_spec;

void  ccm_stats(void);

bool ccm_spawn_child_proc(ccm_child_proc *cp);

void ccm_target_cmd(ccm_str_da sb, ccm_child_proc *cp);
bool ccm_target_needs_rebuild(ccm_target const *t);
c8 **ccm_compile_cmd(ccm_spec *spec, ccm_target const *t);

s32  ccm_spec_schedule_target(ccm_spec *spec, ccm_target *t, ccm_target_array *ta);
void ccm_spec_schedule(ccm_spec *spec, ccm_target_array *ta);

void ccm_spec_build_target(ccm_spec *spec, ccm_target const *t);
void ccm_spec_build(ccm_spec *spec);
void ccm_spec_clean(ccm_spec *spec);

void ccm_bootstrap(ccm_spec *spec);

c8   *ccm_shift_args(s32 *argc, c8 ***argv);

#endif /* CCM_H */

/* #ifdef CCM_IMPLEMENTATION */
#if 1
// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------
s32 ccm_s32_max(s32 a, s32 b)
{
    return a > b ? a : b;
}

s32 ccm_s32_min(s32 a, s32 b)
{
    return a < b ? a : b;
}

// -----------------------------------------------------------------------------
// Logger
// -----------------------------------------------------------------------------
void ccm_panic(c8 const *fmt, ...)
{
    fputs("[PANIC] :: ", stderr);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    abort();
}

void ccm_log(ccm_log_level l, c8 const *fmt, ...)
{
    c8 const *level = NULL;
    va_list ap;
    switch(l) {
    case CCM_LOG_INFO:  level = "[INFO]";  break;
    case CCM_LOG_WARN:  level = "[WARN]";  break;
    case CCM_LOG_DEBUG: level = "[DEBUG]"; break;
    case CCM_LOG_ERROR: level = "[ERROR]"; break;
    case CCM_LOG_NONE:  level = NULL; break;
    default: ccm_unreachable();
    }

    if (level) fprintf(stderr, "%s ", level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

// -----------------------------------------------------------------------------
// Arena
// -----------------------------------------------------------------------------
ccm_arena ccm_arena_init(lll capacity)
{
    ccm_arena arena = {0};
    arena.memory = ccm_malloc(capacity);
    if (arena.memory == NULL) {
        ccm_panic("ERROR :: arena_init: backend malloc: out of memory\n");
    }
    arena.capacity = capacity;
    arena.offset = 0;

    return arena;
}

void* ccm_arena_allocarray(ccm_arena *arena, lll itemsize, lll itemcount, lll align)
{
    assert((align & (align - 1)) == 0 && "align arg must be a power of 2");
    lll nbytes  = itemsize * itemcount;
    if (itemcount != 0 && nbytes / itemcount != itemsize) {
        ccm_panic("ERROR :: arena_alloc: size overflow\n");
    }

    lll padding = -((uintptr_t)(arena->memory + arena->offset)) & (align - 1);
    if (nbytes + padding > arena->capacity - arena->offset) {
        ccm_panic("ERROR :: arena_alloc: out of memory\n");
    }

    arena->offset += padding;
    void *p = arena->memory + arena->offset;
    arena->offset += nbytes;

    return p;
}

void ccm_arena_reset(ccm_arena *arena)
{
    arena->offset = 0;
}

void ccm_arena_log_stats(ccm_arena *arena)
{
    ccm_log(CCM_LOG_INFO, "spec.arena total commited memory = %ld\n", arena->offset);
}

void ccm_arena_deinit(ccm_arena *arena)
{
    ccm_arena_log_stats(arena);
    ccm_free(arena->memory);
    arena->capacity = 0;
    arena->offset = 0;
    arena->memory = NULL;
}

// -----------------------------------------------------------------------------
// Ring Buffer
// -----------------------------------------------------------------------------
ccm_ring_buffer ccm_rb_init(s32 cap)
{
    ccm_ring_buffer rb = {0};
    rb.items = ccm_malloc(cap * sizeof(ccm_rbvalue_t));
        /* ccm_arena_flexalloc(ccm_ring_buffer, items, a, CCM_RINGBUFFER_INITIAL_CAP); */
    rb.cap = cap;
    return rb;
}

void ccm_rb_push(ccm_ring_buffer *rb, ccm_rbvalue_t v)
{
    if (rb->len == rb->cap) {
        rb->cap = 2*rb->cap;
        rb->items = ccm_realloc(rb->items, rb->cap * sizeof(ccm_rbvalue_t)); /* TODO */
    }
    ++rb->len;
    rb->items[rb->write] = v;
    rb->write = (rb->write + 1) % rb->cap;
}

ccm_rbvalue_t ccm_rb_pop(ccm_ring_buffer *rb)
{
    if (rb->len == 0) {
        return NULL;
    }
    --rb->len;
    ccm_rbvalue_t item = rb->items[rb->read];
    rb->read = (rb->read + 1)%rb->cap;
    return item;
}

ccm_rbvalue_t ccm_rb_peek(ccm_ring_buffer const *rb)
{
    if (rb->len == 0) {
        return NULL;
    }
    return rb->items[rb->read];
}

// -----------------------------------------------------------------------------
// Core
// -----------------------------------------------------------------------------
static c8 discard_buffer[CCM_CHILD_PROC_INITIAL_BUFFER_CAP];

#ifdef CCM_STATS
#define fork()          (++fork_count, vfork())
#define exec(...)       (++exec_count, exec(__VA_ARGS__))
#define stat(path, buf) (++stat_count,                                  \
                         ccm_log(CCM_LOG_INFO, "stat(%s)\n", path),     \
                         stat(path, buf))

static s32 fork_count = 0;
static s32 exec_count = 0;
static s32 stat_count = 0;

void ccm_stats(void)
{
    ccm_log(CCM_LOG_DEBUG, "CCM_STATS: fork_count = %d\n", fork_count);
    ccm_log(CCM_LOG_DEBUG, "CCM_STATS: exec_count = %d\n", exec_count);
    ccm_log(CCM_LOG_DEBUG, "CCM_STATS: stat_count = %d\n", stat_count);
}
#endif /* CCM_STATS */

bool ccm_spawn_child_proc(ccm_child_proc *cp)
{
    c8 *pathname = cp->cmd[0];
    c8 **argv    = cp->cmd;

    if (pipe2((int*)&cp->pipe, O_NONBLOCK) < 0) {
        ccm_log(CCM_LOG_ERROR, "target [%s]: pipe2 failed, %s\n",
                cp->target->name,
                strerror(errno));
        return false;
    }
    pid_t cpid = fork();
    switch (cpid) {
    case -1: {
        ccm_log(CCM_LOG_ERROR, "target [%s]: fork failed, %s\n",
                cp->target->name,
                strerror(errno));
        return false;
    }
    case 0: {
        close(cp->pipe.read);
        dup2(cp->pipe.write, STDOUT_FILENO);
        dup2(cp->pipe.write, STDERR_FILENO);
        close(cp->pipe.write);

        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

        if (execvp(pathname, (c8 *const*)argv) < 0) {
            ccm_log(CCM_LOG_ERROR, "execvp failed!\n");
            _exit(1);
        }
        ccm_unreachable();
    }
    default: {
        close(cp->pipe.write);
        cp->pid = cpid;
        cp->time = clock();
    }
    }
    return true;
}

bool ccm_target_needs_rebuild(ccm_target const *t)
{
    struct stat outfile_stat;
    struct stat srcfile_stat;

    time_t output_mtime = 0;

    if (stat(t->name, &outfile_stat) < 0) {
        return true;
    }
    output_mtime = outfile_stat.st_mtime;

    for (s32 i = 0; i < t->sources.len; ++i) {
        if (stat(t->sources.items[i], &srcfile_stat) != -1 &&
            output_mtime < srcfile_stat.st_mtime) {
            return true;
        }
    }
    return false;
}

s32 ccm_spec_schedule_target(ccm_spec *spec, ccm_target *t, ccm_target_array *ta)
{
    if (t->collected) return t->level;
    if (t->visited) {
        ccm_panic("cycle detected: %s\n", t->name);
    }
    t->visited = 1;

    s32 level = 0;
    for (s32 i = 0; i < t->deps.len; ++i) {
        level = ccm_s32_max(level, ccm_spec_schedule_target(spec, t->deps.items[i], ta));
    }
    level += 1;
    t->level = level;
    t->collected = 1;
    ta->items[ta->len++] = t;

    return t->level;
}

void ccm_spec_schedule(ccm_spec *spec, ccm_target_array *ta)
{
    for (s32 i = 0; i < spec->deps.len; ++i) {
        ccm_spec_schedule_target(spec, spec->deps.items[i], ta);
    }
    ccm_log(CCM_LOG_INFO, "job schedule: ");
    for (s32 i = 0; i < ta->len - 1; ++i) {
        ccm_log(CCM_LOG_NONE, "{%s: %d} ~~~> ",
                ta->items[i]->name, ta->items[i]->level);
    }
    ccm_log(CCM_LOG_NONE, "{%s: %d}\n",
            ta->items[ta->len - 1]->name,
            ta->items[ta->len - 1]->level);
}

void ccm_cmd_print(ccm_arena scratch, c8 **cmd)
{
    s32 len = 0;
    c8 **cmd_copy = cmd;
    for (; *cmd_copy; ++cmd_copy) len += strlen(*cmd_copy) + 1;

    c8 *buf = ccm_arena_alloc(c8, &scratch, len + 1);

    s32 remaining = len + 1;
    c8 *itr_buf = buf;

    cmd_copy = cmd;
    for (; *cmd_copy; ++cmd_copy) {
        s32 n = snprintf(itr_buf, remaining, "%s ", *cmd_copy);
        if (n < 0 || n >= remaining) {
            ccm_log(CCM_LOG_ERROR, "CMD: possible buffer overflow\n");
            return;
        }
        itr_buf += n;
        remaining -= n;
    }
    ccm_log(CCM_LOG_INFO, "CMD: %s\n", buf);
}

c8 **ccm_compile_cmd(ccm_spec *spec, ccm_target const *t)
{
    lll cmd_len = 1             /* compiler */
        + spec->common_opts.len
        + t->compiler_opts.len
        + 1                     /* -o */
        + 1                     /* name */
        + t->sources.len
        + t->linker_opts.len
        + 1;                    /* NULL terminator */

    c8 **cmd  = ccm_arena_alloc(c8*, &spec->arena, cmd_len);
    cmd_len = 0;
    cmd[cmd_len++] = spec->compiler;

    for (s32 i = 0; i < spec->common_opts.len; ++i) {
        cmd[cmd_len++] = spec->common_opts.items[i];
    }

    for (s32 i = 0; i < t->compiler_opts.len; ++i) {
        cmd[cmd_len++] = t->compiler_opts.items[i];
    }

    cmd[cmd_len++] = spec->output_flag;
    cmd[cmd_len++] = t->name;

    for (s32 i = 0; i < t->sources.len; ++i) {
        cmd[cmd_len++] = t->sources.items[i];
    }

    for (s32 i = 0; i < t->linker_opts.len; ++i) {
        cmd[cmd_len++] = t->linker_opts.items[i];
    }
    cmd[cmd_len++] = NULL;

    return cmd;
}

void ccm_bootstrap(ccm_spec *spec)
{
    ccm_target const bootstrap = {
        .name = "./ccm",
        .sources = ccm_string_array("ccm.c"),
    };
    if (!ccm_target_needs_rebuild(&bootstrap)) return;

    ccm_child_proc cp = {
        .target = &bootstrap,
        .cmd = ccm_compile_cmd(spec, &bootstrap),
        .report = ccm_arena_alloc(c8, &spec->arena, CCM_CHILD_PROC_INITIAL_BUFFER_CAP),
    };

    memset(cp.report, 0, CCM_CHILD_PROC_INITIAL_BUFFER_CAP); /* NOTE */

    rename(bootstrap.name, "ccm.old");
    ccm_spawn_child_proc(&cp);

    pollfd pfd = {
        .fd = cp.pipe.read,
        .events = POLLIN
    };

    while(1) {
        if (poll(&pfd, 1, -1) == -1) {
            ccm_panic("poll: polling bootstrap child proc failed with error %s\n",
                      strerror(errno));
        }
        if (pfd.revents & POLLIN) {
            c8 *buf = cp.report;
            lll n = 0;
            while ((n = read(pfd.fd, buf, 8192))) {
                if (n == -1 && errno == EINTR) continue;
                if (n == -1 && errno == EAGAIN) break;
                if (n == -1 && errno != EAGAIN) {
                    ccm_log(CCM_LOG_ERROR,
                            "read: reading bootstrap child proc output"
                            "faild with error %s\n",
                            strerror(errno));
                    break;
                }
                buf = discard_buffer;
            }
        }
        if (pfd.revents & POLLHUP) {
            lll n = 0;
            while ((n = read(pfd.fd, discard_buffer, 8192))) {
                if (n == -1 && errno == EINTR) continue;
                if (n == -1 && errno == EAGAIN) break;
                if (n == -1 && errno != EAGAIN) {
                    ccm_log(CCM_LOG_ERROR,
                            "read: reading bootstrap child proc output"
                            "faild with error %s\n",
                            strerror(errno));
                    break;
                }
            }
            assert(close(pfd.fd) == 0);
            break;
        }
    }

    waitpid(cp.pid, &cp.status, 0);
    if (WIFSIGNALED(cp.status)) {
        ccm_log(CCM_LOG_ERROR,
                "bootstrap failed: child proc terminated with signal = %d\n",
                WTERMSIG(cp.status));
        exit(1);
    }

    if (WIFEXITED(cp.status)) {
        ccm_cmd_print(spec->arena, cp.cmd);
        ccm_log(CCM_LOG_NONE, "%s", cp.report);

        if (cp.status != 0) {
            ccm_log(CCM_LOG_ERROR, "bootstrap failed, child proc exit status = %d\n",
                    WEXITSTATUS(cp.status));
            rename("ccm.old", bootstrap.name);
            exit(1);
        }

        ccm_log(CCM_LOG_INFO, "bootstrap succeeded\n");

        execvp(bootstrap.name, spec->argv);

        ccm_panic("bootstrap: execvp failed: %s\n", strerror(errno));
        exit(1);
    }
}

void ccm_spec_build(ccm_spec *spec)
{
    if (spec->arena.memory == NULL) spec->arena = ccm_arena_init(CCM_ARENA_DEFAULT_CAP);

    /* bootstrap */ {
        ccm_bootstrap(spec); /* this function will not return in case we need to rebuild */
        ccm_log(CCM_LOG_INFO, "ccm upto date, skip bootstrapping\n");
    }

    ccm_as_scratch_arena(spec->arena) {
        /* useful for cycle detection */
        ccm_target_array dfs = {0};
        dfs.items = ccm_arena_alloc(ccm_target*, &spec->arena, spec->deps.len);
        ccm_spec_schedule(spec, &dfs);
    }
    /* TODO remove duplicates from spec->deps array */
    /* construct the reverse edges */ {
        for (s32 i = 0; i < spec->deps.len; ++i) {
            ccm_target *t = spec->deps.items[i];
            for (s32 j = 0; j < t->deps.len; ++j) {
                ccm_target *dep = t->deps.items[j];
                dep->revdeps.len += 1;
            }
        }

        for (s32 i = 0; i < spec->deps.len; ++i) {
            ccm_target *t = spec->deps.items[i];
            t->revdeps.items = ccm_arena_alloc(ccm_target *, &spec->arena, t->revdeps.len);
            t->revdeps.len = 0;
        }

        for (s32 i = 0; i < spec->deps.len; ++i) {
            ccm_target *t = spec->deps.items[i];
            for (s32 j = 0; j < t->deps.len; ++j) {
                ccm_target *dep = t->deps.items[j];
                dep->revdeps.items[dep->revdeps.len++] = t;
            }
        }
    }

    ccm_ring_buffer ready_queue = ccm_rb_init(CCM_RINGBUFFER_INITIAL_CAP);
    for (s32 i = 0; i < spec->deps.len; ++i) {
        ccm_target *t = spec->deps.items[i];
        if (t->deps.len == 0) {
            ccm_rb_push(&ready_queue, t);
        }
    }


    s32 remaining_targets = spec->deps.len;
    s32 n_running_jobs = 0;
    s32 timeout = 100;

    pollfd         *pfds = ccm_arena_alloc(pollfd, &spec->arena, spec->j);
    ccm_child_proc *cps  = ccm_arena_alloc(ccm_child_proc, &spec->arena, spec->j);

    /* initialize report buffers */
    for (s32 i = 0; i < spec->j; ++i) {
        cps[i].report = ccm_arena_alloc(c8, &spec->arena, CCM_CHILD_PROC_INITIAL_BUFFER_CAP);
    }

    ccm_log(CCM_LOG_NONE,
            "========================================================================================================================\n");
    while (remaining_targets > 0) {
        while (n_running_jobs < spec->j && 0 < ready_queue.len) {
            ccm_target *t = ccm_rb_pop(&ready_queue);
            cps[n_running_jobs].target = t;
            cps[n_running_jobs].cmd = ccm_compile_cmd(spec, t);
            cps[n_running_jobs].id = n_running_jobs;
            memset(cps[n_running_jobs].report, 0, CCM_CHILD_PROC_INITIAL_BUFFER_CAP); /* NOTE */

            ccm_spawn_child_proc(&cps[n_running_jobs]);

            pfds[n_running_jobs].fd     = cps[n_running_jobs].pipe.read;
            pfds[n_running_jobs].events = POLLIN;

            ++n_running_jobs;
        }
        s32 nready = poll(pfds, n_running_jobs, timeout);
        if (nready == -1) {
            ccm_panic("poll: target poll failed with error %s\n", strerror(errno));
        }

        for (s32 i = 0; nready && i < n_running_jobs; ++i) {
            if (pfds[i].revents & POLLIN) {
                c8 *buf = cps[i].report;
                lll n = 0;
                while ((n = read(pfds[i].fd, buf, 8192))) {
                    if (n == -1 && errno == EINTR) continue;
                    if (n == -1 && errno == EAGAIN) break;
                    if (n == -1 && errno != EAGAIN) {
                        ccm_log(CCM_LOG_ERROR,
                                "read: reading bootstrap child proc output"
                                "faild with error %s\n",
                                strerror(errno));
                        break;
                    }
                    buf = discard_buffer;
                }
            }
            if (pfds[i].revents & POLLHUP) {
                lll n = 0;
                while ((n = read(pfds[i].fd, discard_buffer, 8192))) {
                    if (n == -1 && errno == EINTR) continue;
                    if (n == -1 && errno == EAGAIN) break;
                    if (n == -1 && errno != EAGAIN) {
                        ccm_log(CCM_LOG_ERROR,
                                "read: reading bootstrap child proc output"
                                "faild with error %s\n",
                                strerror(errno));
                        break;
                    }
                }

                int ret = waitpid(cps[i].pid, &cps[i].status, WNOHANG);
                if (ret == 0) {
                    // Child not dead yet, skip this cycle
                    continue;
                }
                if (ret == -1) {
                    ccm_log(CCM_LOG_ERROR, "waitpid failed on pid %d: %s\n", cps[i].pid, strerror(errno));
                    continue;
                }

                assert(close(pfds[i].fd) == 0);
                if (WIFSIGNALED(cps[i].status)) {
                    ccm_log(CCM_LOG_ERROR,
                            "Target [%s] failed: child proc terminated with signal = %d\n",
                            cps[i].target->name, WTERMSIG(cps[i].status));
                }

                if (WIFEXITED(cps[i].status)) {
                    ccm_log(CCM_LOG_INFO, "job [%d] time: %f ms\n", cps[i].id,
                            1000 * (double)(clock() - cps[i].time)/CLOCKS_PER_SEC);
                    ccm_cmd_print(spec->arena, cps[i].cmd);
                    ccm_log(CCM_LOG_NONE, "%s", cps[i].report);
                    ccm_log(CCM_LOG_NONE,
                            "========================================================================================================================\n");
                }
                /* update the ready queue with targets in current target depedent list */
                for (s32 d = 0; d < cps[i].target->revdeps.len; ++d) {
                    ccm_target *rt = cps[i].target->revdeps.items[d];
                    --rt->deps.len;
                    if (rt->deps.len == 0) {
                        ccm_rb_push(&ready_queue, rt);
                    }
                }

                /* simulate removing child proc and associated
                 * pollfd by swapping the last one
                 */
                ccm_swap(ccm_child_proc, cps[i], cps[n_running_jobs - 1]);
                ccm_swap(pollfd, pfds[i],pfds[n_running_jobs - 1]);
                --i;

                --n_running_jobs;
                --nready;
                --remaining_targets;
            }
        }
    }

#ifdef CCM_STATS
    ccm_stats();
#endif  /* CCM_STATS */
    ccm_da_deinit(&ready_queue);
    ccm_arena_deinit(&spec->arena);
}

void ccm_spec_clean(ccm_spec *b)
{
    for (s32 i = 1; i < b->deps.len; ++i) {
        if (remove(b->deps.items[i]->name) == -1) {
            ccm_log(CCM_LOG_ERROR, "rm %s failed!\n", b->deps.items[i]->name);
            continue;
        }
        b->deps.items[i]->visited = 0;
        b->deps.items[i]->collected = 0;
    }
}

c8* ccm_shift_args(s32 *argc, c8 ***argv)
{
    c8 *r = (*argv)[0];
    --(*argc);
    ++(*argv);
    return r;
}

#endif /* CCM_IMPLEMENTATION */

/* TODOs
 * proper poll handling:
 *   refactor the poll handling code into reusable components
 * examples:
 *   write usage examples, implement unit tests for functions
 * memory management:
 *   improve memory management and decide on arenas vs malloc family
 * logging:
 *   improve job logging, namely CMD logging and other debug info, and add a verbosity flag for debugging
 */

/* DONEs
 * Redirection:
 *   Proper output redirection with pipes to ensure deterministic output order
 * Correct Handling of deps with respect to forking:
 *   forcing targets-with-deps to build in correct order.
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
 * Correct bootstrapping stat-syscall handling:
 *   by adding a bootstrap_target as a dependency for the spec
 */
