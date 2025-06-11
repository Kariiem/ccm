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

typedef struct ccm_arena         ccm_arena;

typedef struct ccm_str8_buf      ccm_str8_buf;
typedef struct ccm_str8_view     ccm_str8_view;
typedef struct ccm_str8_array    ccm_str8_array;
typedef struct ccm_str8_dynarray ccm_str8_dynarray;

typedef struct ccm_target*       ccm_rbvalue_t;
typedef struct ccm_ring_buffer   ccm_ring_buffer;

typedef struct pollfd            pollfd;
typedef struct ccm_pipe          ccm_pipe;
typedef struct ccm_childproc     ccm_childproc;
typedef struct ccm_childproc_manager ccm_childproc_manager;

typedef struct ccm_target        ccm_target;
typedef struct ccm_target_array  ccm_target_array;
typedef struct ccm_spec          ccm_spec;


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
enum {
    CCM_NONE_LOG,
    CCM_INFO_LOG,
    CCM_WARN_LOG,
    CCM_DEBUG_LOG,
    CCM_ERROR_LOG,
};

void ccm_panic(c8 const *fmt, ...) CCM_ATTR_PRINTF(1, 2);
void ccm_log(s32 l, c8 const *fmt, ...) CCM_ATTR_PRINTF(2, 3);

// -----------------------------------------------------------------------------
// [4] Arena
// -----------------------------------------------------------------------------
#ifndef CCM_ARENA_DEFAULT_CAP
#define CCM_ARENA_DEFAULT_CAP (64 * 1024 * 1024) /* 64mb */
#endif /* CCM_ARENA_DEFAULT_CAP */

struct ccm_arena {
    u8 *memory;
    lll cap;
    lll off;
};

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
struct ccm_ring_buffer {
    s32 read;
    s32 write;
    s32 cap;
    s32 len;
    ccm_rbvalue_t *items;
};

void          ccm_rb_push(ccm_ring_buffer *rb, ccm_rbvalue_t v);
ccm_rbvalue_t ccm_rb_pop(ccm_ring_buffer *rb);
ccm_rbvalue_t ccm_rb_peek(ccm_ring_buffer const *rb);

// -----------------------------------------------------------------------------
// [6] Dynamic Array
// -----------------------------------------------------------------------------
enum /* whether to memset with 0 or not */ {
    CCM_ZERO_MEM,
    CCM_NOZERO_MEM,
};

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

#define ccm_da_init(da, capacity, flag)         \
    do {                                        \
        lll cap = (capacity);                   \
        lll item_size = sizeof((da)->items[0]); \
        lll nbytes = cap * item_size;           \
        (da)->items = ccm_malloc(nbytes);       \
        (da)->cap   = cap;                      \
        (da)->len   = 0;                        \
        (da)->items = flag == CCM_ZERO_MEM ?    \
            memset((da)->items, 0, nbytes) :    \
            (da)->items;                        \
    } while(0)

#define ccm_da_grow(da, newcap)                                 \
    do {                                                        \
        lll cap = (newcap);                                     \
        if ((da)->cap > cap) break;   /* no shrink */           \
        lll item_size = sizeof((da)->items[0]);                 \
        void *p = ccm_realloc((da)->items, (cap * item_size));  \
        if (p == NULL) {                                        \
            ccm_panic("ccm_realloc failed at %s: %d",           \
                      __FILE_NAME__, __LINE__);                 \
        }                                                       \
        (da)->items = p;                                        \
        (da)->cap   = cap;                                      \
    } while(0)


#define ccm_da_append(da, item)                                 \
    do {                                                        \
        lll cap = (da)->cap;                                    \
        lll len = (da)->len;                                    \
        if (len == cap) {                                       \
            cap = cap == 0 ? CCM_DA_INITIAL_CAP : cap * 2;      \
            ccm_da_grow(da, cap);                               \
        }                                                       \
        (da)->items[len] = (item);                              \
        ++(da)->len;                                            \
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
// [7] Strings
// -----------------------------------------------------------------------------
struct ccm_str8_buf {
    lll cap;
    lll len;
    c8 *items;
};
struct ccm_str8_view {
    lll len;
    c8 *items;
};
struct ccm_str8_array {
    lll len;
    c8 **items;
};
struct ccm_str8_dynarray {
    lll cap;
    lll len;
    c8 const**items;
};

// -----------------------------------------------------------------------------
// [8] ChildProc
// -----------------------------------------------------------------------------
#ifndef CCM_CHILDPROC_REPORT_BUF_CAP
#define CCM_CHILDPROC_REPORT_BUF_CAP (8*1024) /* 8kb */
#endif /* CCM_CHILDPROC_REPORT_BUF_CAP */

enum {
    CCM_WAIT_ERROR = 1,
    CCM_WAIT_DONE,
    CCM_WAIT_PENDING,
};
struct ccm_pipe {
    s32 read;
    s32 write;
};
struct ccm_childproc {
    pid_t pid;
    s32 id;
    s32 status;
    ccm_pipe pipe;
    c8 **cmd;
    ccm_str8_buf report;
    clock_t time;
    ccm_target const *target;
};
struct ccm_childproc_manager {
    ccm_childproc *cps;
    pollfd        *pfds;
    lll           len;
};

bool ccm_childproc_fork(ccm_childproc *cp);
void ccm_childproc_read(ccm_childproc *cp, s32 revents);
s32  ccm_childproc_wait(ccm_childproc *cp);

void ccm_childproc_report(ccm_childproc *cp);

// -----------------------------------------------------------------------------
// [9] Build Specification & Build Targets
// -----------------------------------------------------------------------------
#define ccm_str8_array_len(...) ccm_countof(((c8 *[]){__VA_ARGS__}))
#define ccm_str8_array(...)                     \
    (ccm_str8_array) {                          \
        .len = ccm_str8_array_len(__VA_ARGS__), \
            .items = (c8 *[]){ __VA_ARGS__ },   \
            }

#define X__deps_array_len(...) ccm_countof(((ccm_target *[]){__VA_ARGS__}))
#define X__deps_array(...)                              \
    (ccm_target_array) {                                \
        .len = X__deps_array_len(__VA_ARGS__),          \
            .items = (ccm_target *[]){ __VA_ARGS__ },   \
            }

#define ccm_deps_array_len(...) X__deps_array_len(__VA_ARGS__)
#define ccm_deps_array(...)     X__deps_array(__VA_ARGS__)

struct ccm_target_array {
    lll len;
    ccm_target **items;
};
struct ccm_target {
    c8 *name;
    ccm_str8_array sources;
    ccm_str8_array compiler_opts;
    ccm_str8_array linker_opts;
    ccm_target_array deps;
    ccm_target_array revdeps;

    s32 level;
    s32 visited;
    s32 collected;
};
struct ccm_spec {
    c8 *compiler;
    c8 *output_flag;
    ccm_str8_array common_opts;
    ccm_target_array deps;
    ccm_arena arena;
    c8 **argv;
    s32 argc;
    s32 j;
};
void  ccm_stats(void);

void ccm_target_cmd(ccm_str8_dynarray sb, ccm_childproc *cp);
bool ccm_target_needs_rebuild(ccm_target const *t);
c8 **ccm_compile_cmd(ccm_spec *spec, ccm_target const *t);

s32  ccm_spec_schedule_target(ccm_spec *spec, ccm_target *t, ccm_target_array *ta);
void ccm_spec_schedule(ccm_spec *spec, ccm_target_array *ta);

void ccm_spec_build_target(ccm_spec *spec, ccm_target const *t);
void ccm_spec_build(ccm_spec *spec);
void ccm_spec_clean(ccm_spec *spec);

void ccm_bootstrap(ccm_spec *spec, s32 timeout);

c8   *ccm_shift_args(s32 *argc, c8 ***argv);

void  ccm_cmd_print(ccm_arena a, c8 **cmd);

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

void ccm_log(s32 l, c8 const *fmt, ...)
{
    c8 const *level = NULL;
    va_list ap;
    switch(l) {
    case CCM_INFO_LOG:  level = "[INFO]";  break;
    case CCM_WARN_LOG:  level = "[WARN]";  break;
    case CCM_DEBUG_LOG: level = "[DEBUG]"; break;
    case CCM_ERROR_LOG: level = "[ERROR]"; break;
    case CCM_NONE_LOG:  level = NULL; break;
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
    arena.cap = capacity;
    arena.off = 0;

    return arena;
}

void* ccm_arena_allocarray(ccm_arena *arena, lll itemsize, lll itemcount, lll align)
{
    assert((align & (align - 1)) == 0 && "align arg must be a power of 2");
    lll nbytes  = itemsize * itemcount;
    if (itemcount != 0 && nbytes / itemcount != itemsize) {
        ccm_panic("ERROR :: arena_alloc: size overflow\n");
    }

    lll padding = -((uintptr_t)(arena->memory + arena->off)) & (align - 1);
    if (nbytes + padding > arena->cap - arena->off) {
        ccm_panic("ERROR :: arena_alloc: out of memory\n");
    }

    arena->off += padding;
    void *p = arena->memory + arena->off;
    arena->off += nbytes;

    return p;
}

void ccm_arena_reset(ccm_arena *arena)
{
    arena->off = 0;
}

void ccm_arena_log_stats(ccm_arena *arena)
{
    ccm_log(CCM_INFO_LOG, "spec.arena total commited memory = %ld\n", arena->off);
}

void ccm_arena_deinit(ccm_arena *arena)
{
    ccm_arena_log_stats(arena);
    ccm_free(arena->memory);
    arena->cap = 0;
    arena->off = 0;
    arena->memory = NULL;
}
// -----------------------------------------------------------------------------
// Strings
// -----------------------------------------------------------------------------
/* TODO
 * ccm_concat: takes a list of strings and concats them into a single string seperated by spaces.
 */
c8 *ccm_fmt(ccm_arena *arena, char const *fmt, ...)
{
    va_list ap;
    lll len = 0;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (len < 0) return NULL;

    lll mark = arena->off;
    void *p = ccm_arena_alloc(c8, arena, len + 1); /* NOTE +1 */

    va_start(ap, fmt);
    len = vsnprintf(p, len + 1, fmt, ap);
    va_end(ap);

    if (len < 0) {
        arena->off = mark;
        return NULL;
    }
    return p;
}

c8 *ccm_concat(ccm_arena *arena, c8 **str8s)
{
    if (!str8s || !*str8s) return "";

    s32 len = 0;
    s32 count = 0;
    for (c8 **s = str8s; *s; ++s, ++count) {
        len += strlen(*s);
    }
    len += count - 1;  // spaces between strings
    len += 1;          // null terminator

    c8 *buf = ccm_arena_alloc(c8, arena, len);
    c8 *p = buf;

    for (s32 i = 0; i < count; ++i) {
        s32 slen = strlen(str8s[i]);
        memcpy(p, str8s[i], slen);
        p += slen;

        if (i != count - 1) {
            *p++ = ' ';
        }
    }
    *p = '\0';
    return buf;
}

c8 *ccm_concat_with_snprintf(ccm_arena *arena, c8 **str8s)
{
    s32 len = 0;
    s32 count = 0;
    c8 **str8s_copy = str8s;

    for (; *str8s_copy; ++str8s_copy, ++count) len += strlen(*str8s_copy) + 1;

    lll mark = arena->off;
    c8 *buf = ccm_arena_alloc(c8, arena, len);
    s32 remaining = len;
    c8 *itr_buf = buf;

    for (s32 i = 0; i < count - 1; ++i) {
        s32 n = snprintf(itr_buf, remaining, "%s ", str8s[i]);
        if (n < 0 || n >= remaining) {
            ccm_log(CCM_ERROR_LOG, "CMD: possible buffer overflow\n");
            arena->off = mark;
            return NULL;
        }
        itr_buf += n;
        remaining -= n;
    }
    s32 n = snprintf(itr_buf, remaining, "%s", str8s[count-1]);
    if (n < 0 || n >= remaining) {
        ccm_log(CCM_ERROR_LOG, "CMD: possible buffer overflow\n");
        arena->off = mark;
        return NULL;
    }

    return buf;
}


// -----------------------------------------------------------------------------
// Ring Buffer
// -----------------------------------------------------------------------------
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
// [8] ChildProc
// -----------------------------------------------------------------------------
bool ccm_childproc_fork(ccm_childproc *cp)
{
    c8 *pathname = cp->cmd[0];
    c8 **argv    = cp->cmd;
    pid_t cpid   = fork();

    switch (cpid) {
    case -1: {
        ccm_log(CCM_ERROR_LOG, "target [%s]: fork failed, %s\n",
                cp->target->name,
                strerror(errno));
        return false;
    }
    case 0: {
        close(cp->pipe.read);
        dup2(cp->pipe.write, STDOUT_FILENO);
        dup2(cp->pipe.write, STDERR_FILENO);
        close(cp->pipe.write);

        if (execvp(pathname, (c8 *const*)argv) < 0) {
            ccm_log(CCM_ERROR_LOG, "execvp failed!\n");
            exit(1);
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

void ccm_childproc_read(ccm_childproc *cp, s32 revents)
{
    /* only called on fds where poll revents
     * has POLLIN | POLLHUP
     * or when child proc has exited
     */
    ccm_unused(revents);
    c8 *buf = cp->report.items + cp->report.len;
    lll read_len = cp->report.cap - cp->report.len;
    lll n = 0;
    for (s32 i = 0; (n = read(cp->pipe.read, buf, read_len)) ; ++i) {
#ifdef CCM_DEBUG_READ_AFTER_POLL
        ccm_log(CCM_DEBUG_LOG, "read iteration [%d], read %ld bytes\n", i, n);
#endif
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EAGAIN) {
#ifdef CCM_DEBUG_READ_AFTER_POLL
            /* DONE investigate this matter more
             * revents has POLLIN set, yet read fails with EAGAIN, so the pipe is
             * empty, this is an interaction between poll and O_NONBLOCK, afaiu.
             *
             * Actually using pipe2(..., 0) never hits this path
             * however I will keep using O_NONBLOCK and handle EAGAIN
             * =====================================================================
             *
             * This turned out to be due to read being in a loop (to drain the pipe)
             * making the pipe empty when there is no data to read, returning -1 and
             * setting errno = EAGAIN
             *
             * The reason why this never happens when O_NONBLOCK is cleared, is because
             * read blocks until there is data availabe, and since we do it in a loop,
             * it actually block the whole poll ~~~> read pipeline, and actually waits
             * for the writing end to close.
             */
            if (revents & POLLIN) {
                ccm_log(CCM_DEBUG_LOG, "revents: POLLIN,   (%d)\n", i);
            }
            if (revents & POLLHUP) {
                ccm_log(CCM_DEBUG_LOG, "revents: POLLHUP, (%d)\n", i);

            }
#endif
            break;
        }
        if (n == -1) {
            ccm_log(CCM_ERROR_LOG, "read: child %d failed: %s\n",
                    cp->pid, strerror(errno));
            break;
        }
        cp->report.len += n;
        buf += n;
        read_len -= n;
        if (read_len == 0) {
            ccm_da_grow(&cp->report, cp->report.cap * 2);
            buf = cp->report.items + cp->report.len;
            read_len = cp->report.cap - cp->report.len;
        }
    }
}

s32 ccm_childproc_wait(ccm_childproc *cp)
{
    s32 ret = waitpid(cp->pid, &cp->status, WNOHANG);
    c8 *tname = cp->target->name;

    if (ret == 0) return CCM_WAIT_PENDING;
    if (ret == -1) {
        if (errno == EINTR) return CCM_WAIT_PENDING;
        if (errno == ECHILD) ccm_panic("Target[%s]: invalid child process\n", tname);

        ccm_log(CCM_ERROR_LOG,"Target [%s]: waitpid failed on pid %d: %s\n",
                tname,
                cp->pid,
                strerror(errno));
        return CCM_WAIT_ERROR;
    }

    if (WIFEXITED(cp->status)) {
        if (cp->status == EXIT_SUCCESS) {
            return CCM_WAIT_DONE;
        }
        ccm_log(CCM_ERROR_LOG, "Target [%s]: child proc exit status = %d\n",
                tname,
                WEXITSTATUS(cp->status));
    } else if (WIFSIGNALED(cp->status)) {
        ccm_log(CCM_ERROR_LOG, "Target [%s]: child proc terminated with signal = %d\n",
                tname,
                WTERMSIG(cp->status));
    }

    if (close(cp->pipe.read) != 0) {
        ccm_log(CCM_ERROR_LOG,"Target [%s]: close failed on fd %d: %s\n",
                tname,
                cp->pipe.read,
                strerror(errno));
    }

    return CCM_WAIT_ERROR;
}

void ccm_childproc_report(ccm_childproc *cp)
{
    write(STDOUT_FILENO, cp->report.items, cp->report.len);
    memset(cp->report.items, 0, cp->report.len);
    cp->report.len = 0;
}

// -----------------------------------------------------------------------------
// Core
// -----------------------------------------------------------------------------

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
    ccm_log(CCM_INFO_LOG, "job schedule: ");
    for (s32 i = 0; i < ta->len - 1; ++i) {
        ccm_log(CCM_NONE_LOG, "{%s: %d} ~~~> ",
                ta->items[i]->name, ta->items[i]->level);
    }
    ccm_log(CCM_NONE_LOG, "{%s: %d}\n",
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
            ccm_log(CCM_ERROR_LOG, "CMD: possible buffer overflow\n");
            return;
        }
        itr_buf += n;
        remaining -= n;
    }
    ccm_log(CCM_INFO_LOG, "CMD: %s\n", buf);
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

bool ccm_childprocs_monitor(ccm_childproc *cp, pollfd *pfd, s32 timeout)
{
    ccm_childproc_fork(cp);

    for (s32 i = 0; ; ++i) {
#ifdef CCM_DEBUG_READ_AFTER_POLL
        ccm_log(CCM_DEBUG_LOG, "bootstrap polling loop iteration %d\n", i);
#endif
        s32 ready = poll(pfd, 1, timeout);
        if (ready == -1) {
            ccm_panic("poll: polling bootstrap child proc failed with error %s\n",
                      strerror(errno));
        }

        if (ready > 0 && (pfd->revents & (POLLIN | POLLHUP))) {
            ccm_childproc_read(cp, pfd->revents);
            if (pfd->revents & POLLHUP) {
                break;
            }
        }
    }

    s32 ev = 0;
    do {
        ev = ccm_childproc_wait(cp);
    } while (ev == CCM_WAIT_PENDING);

    return ev == CCM_WAIT_DONE;
}

void ccm_bootstrap(ccm_spec *spec, s32 timeout)
{
    ccm_target const bootstrap = {
        .name = "./ccm",
        .sources = ccm_str8_array("ccm.c", "ccm.h"),
    };
    if (!ccm_target_needs_rebuild(&bootstrap)) return;

    ccm_childproc cp = {
        .target = &bootstrap,
        .cmd = ccm_compile_cmd(spec, &bootstrap),
    };

    c8 *oldname = ccm_fmt(&spec->arena, "%s.old", bootstrap.name);

    ccm_da_init(&cp.report, CCM_CHILDPROC_REPORT_BUF_CAP, CCM_ZERO_MEM);

    ccm_as_scratch_arena(spec->arena) {
        ccm_log(CCM_INFO_LOG, "CMD: %s\n", ccm_concat(&spec->arena, cp.cmd));
    }

    {
        if (pipe2((int*)&cp.pipe, 0) < 0) {
            ccm_panic("bootstrap: pipe2 failed, %s\n",
                      strerror(errno));
        }
        pollfd pfd = {
            .fd = cp.pipe.read,
            .events = POLLIN
        };

        rename(bootstrap.name, oldname);
        s32 ok = ccm_childprocs_monitor(&cp, &pfd, timeout);

        ccm_childproc_report(&cp);

        if (!ok) {
            rename(oldname, bootstrap.name);
            exit(1);
        }
    }

    ccm_da_deinit(&cp.report);
    ccm_log(CCM_INFO_LOG, "bootstrap succeeded\n");

    execvp(bootstrap.name, spec->argv);

    ccm_panic("bootstrap: execvp failed: %s\n", strerror(errno));
    exit(1);
}

void ccm_compute_dependents(ccm_spec *spec)
{
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

ccm_ring_buffer *ccm_compute_ready_queue(ccm_spec *spec)
{
    ccm_ring_buffer *ready_queue = ccm_arena_alloc(ccm_ring_buffer, &spec->arena);
    ready_queue->items = ccm_arena_alloc(ccm_rbvalue_t, &spec->arena, spec->deps.len);
    ready_queue->cap = spec->deps.len;
    ready_queue->len = 0;
    ready_queue->read = 0;
    ready_queue->write = 0;

    for (s32 i = 0; i < spec->deps.len; ++i) {
        ccm_target *t = spec->deps.items[i];
        if (t->deps.len == 0) {
            ccm_rb_push(ready_queue, t);
        }
    }
    return ready_queue;
}

void ccm_spec_build(ccm_spec *spec)
{
    s32 timeout = 100;
    s32 remaining_targets = spec->deps.len;
    s32 n_running_jobs = 0;


    /* TODO remove duplicates from spec->deps array */
    if (spec->arena.memory == NULL) spec->arena = ccm_arena_init(CCM_ARENA_DEFAULT_CAP);

    /* bootstrap */ {
        /* this function will not return in case we need to rebuild */
        ccm_bootstrap(spec, timeout);
        ccm_log(CCM_INFO_LOG, "ccm upto date, skip bootstrapping\n");
    }


    exit(0);
    ccm_as_scratch_arena(spec->arena) {
        /* useful for cycle detection */
        ccm_target_array dfs = {0};
        dfs.items = ccm_arena_alloc(ccm_target*, &spec->arena, spec->deps.len);
        ccm_spec_schedule(spec, &dfs);
    }

    ccm_ring_buffer *ready_queue = ccm_compute_ready_queue(spec);

    /* compute reverse edges */
    ccm_compute_dependents(spec);

    /* allocate child procs and associated pollfd struct */
    pollfd        *pfds = ccm_arena_alloc(pollfd, &spec->arena, spec->j);
    ccm_childproc *cps  = ccm_arena_alloc(ccm_childproc, &spec->arena, spec->j);
    s32            *ws  = ccm_arena_alloc(s32, &spec->arena, spec->j);


    /* initialize report buffers */
    for (s32 i = 0; i < spec->j; ++i) {
        ccm_da_init(&cps[i].report, CCM_CHILDPROC_REPORT_BUF_CAP, CCM_ZERO_MEM);
    }

    ccm_log(CCM_NONE_LOG,
            "============================================================"
            "============================================================\n");
    while (remaining_targets > 0) {
        while (n_running_jobs < spec->j && 0 < ready_queue->len) {
            ccm_target *t = ccm_rb_pop(ready_queue);

            cps[n_running_jobs].target = t;
            cps[n_running_jobs].cmd = ccm_compile_cmd(spec, t);
            cps[n_running_jobs].id = n_running_jobs;

            /* forks the child and sets up the pipe */
            ccm_childproc_fork(&cps[n_running_jobs]);

            pfds[n_running_jobs].fd     = cps[n_running_jobs].pipe.read;
            pfds[n_running_jobs].events = POLLIN;

            ++n_running_jobs;
        }
        s32 nready = poll(pfds, n_running_jobs, timeout);
        if (nready == -1) {
            ccm_panic("poll: target poll failed with error %s\n", strerror(errno));
        }

        for (s32 i = 0; i < n_running_jobs; ++i) {
            ws[i] = waitpid(cps[i].pid, &cps[i].status, WNOHANG);

            if (ws[i] > 0 || (pfds[i].revents & (POLLIN | POLLHUP))) {
                c8 *buf = cps[i].report.items;
                lll read_len = cps[i].report.cap - cps[i].report.len;
                lll n;
                while (true) {
                    n = read(pfds[i].fd, buf, read_len);
                    if (n == -1 && errno == EINTR) continue;
                    if (n == -1 && errno == EAGAIN) {
                        if (ws[i] > 0) continue; // Retry if child exited
                        if (pfds[i].revents & POLLHUP) continue; // Retry on POLLHUP
                        break; // No data, wait for next poll
                    }
                    if (n == -1) {
                        ccm_log(CCM_ERROR_LOG, "read: child %d failed: %s\n",
                                cps[i].pid, strerror(errno));
                        break;
                    }
                    if (n == 0 && (ws[i] > 0 || (pfds[i].revents & POLLHUP))) {
                        break; // EOF confirmed
                    }
                    if (n > 0) {
                        cps[i].report.len += n;
                        buf += n;
                        read_len -= n;
                        if (read_len == 0) {
                            ccm_da_grow(&cps[i].report, cps[i].report.cap * 2);
                            buf = cps[i].report.items + cps[i].report.len;
                            read_len = cps[i].report.cap - cps[i].report.len;
                        }
                    }
                }
            }

            if (pfds[i].revents & (POLLERR | POLLNVAL)) {
                ccm_log(CCM_ERROR_LOG, "poll error on fd %d: revents = %x\n", pfds[i].fd, pfds[i].revents);
            }

            if (ws[i] == 0) continue;
            if (ws[i] == -1) {
                ccm_log(CCM_ERROR_LOG, "Target [%s]: waitpid failed on pid %d: %s\n",
                        cps[i].target->name,
                        cps[i].pid,
                        strerror(errno));
                continue;
            }
            assert(close(pfds[i].fd) == 0); /* NOTE */

            if (WIFSIGNALED(cps[i].status)) {
                ccm_log(CCM_ERROR_LOG,
                        "Target [%s] failed: child proc terminated with signal = %d\n",
                        cps[i].target->name,
                        WTERMSIG(cps[i].status));
            }
            if (WIFEXITED(cps[i].status)) {
                ccm_log(CCM_INFO_LOG, "job [%d] time: %f ms\n", cps[i].id,
                        1000 * (double)(clock() - cps[i].time)/CLOCKS_PER_SEC);
                ccm_cmd_print(spec->arena, cps[i].cmd);
                write(STDOUT_FILENO, cps[i].report.items, cps[i].report.len);
                ccm_log(CCM_NONE_LOG,
                        "============================================================"
                        "============================================================\n");
                /* reset report buffers for future use */
                memset(cps[i].report.items, 0, cps[i].report.cap);
                cps[i].report.len = 0;
            }
            /* update the ready queue with targets in current target depedent list */
            for (s32 d = 0; d < cps[i].target->revdeps.len; ++d) {
                ccm_target *rt = cps[i].target->revdeps.items[d];
                --rt->deps.len;
                if (rt->deps.len == 0) {
                    ccm_rb_push(ready_queue, rt);
                }
            }

            /* simulate removing child proc and associated
             * pollfd by swapping the last one
             */
            ccm_swap(ccm_childproc, cps[i], cps[n_running_jobs - 1]);
            ccm_swap(pollfd, pfds[i],pfds[n_running_jobs - 1]);
            --i;
            --n_running_jobs;
            --remaining_targets;
        }
    }

#ifdef CCM_STATS
    ccm_stats();
#endif  /* CCM_STATS */

    for (s32 i = 0; i < spec->j; ++i) {
        ccm_da_deinit(&cps[i].report);
    }
    ccm_arena_deinit(&spec->arena);
}

void ccm_spec_clean(ccm_spec *b)
{
    for (s32 i = 1; i < b->deps.len; ++i) {
        if (remove(b->deps.items[i]->name) == -1) {
            ccm_log(CCM_ERROR_LOG, "rm %s failed!\n", b->deps.items[i]->name);
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
