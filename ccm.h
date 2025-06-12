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

typedef enum   ccm_event         ccm_event;
typedef struct pollfd            pollfd;
typedef struct ccm_pipe          ccm_pipe;
typedef struct ccm_childproc     ccm_childproc;
typedef struct ccm_proc_mgr      ccm_proc_mgr;

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
    CCM_LOG_NONE = 0,
    CCM_LOG_INFO,
    CCM_LOG_WARN,
    CCM_LOG_DEBUG,
    CCM_LOG_ERROR,
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

enum ccm_event {
    CCM_EVENT_WAIT_ERROR   = 1 << 0,
    CCM_EVENT_WAIT_TERM    = 1 << 1,
    CCM_EVENT_WAIT_DONE    = 1 << 2,
    CCM_EVENT_WAIT_PENDING = 1 << 3,

    CCM_EVENT_POLLIN       = 1 << 10,
    CCM_EVENT_POLLHUP      = 1 << 11,
    CCM_EVENT_POLLERR      = 1 << 12,

    CCM_EVENT_CLOSE_ERR    = 1 << 20,

};

struct ccm_pipe {
    s32 read;
    s32 write;
};
struct ccm_childproc {
    pid_t pid;
    s32 status;
    clock_t time;
    ccm_pipe pipe;

    c8 **cmd;
    ccm_str8_buf report;
    ccm_target const *target;
};

struct ccm_proc_mgr {
    s32           maxjobs;
    s32           nrunning;
    s32           timeout;
    ccm_spec      *spec;
    ccm_event     *evs;
    ccm_childproc *cps;
    pollfd        *pfds;
};

ccm_proc_mgr ccm_proc_mgr_init(ccm_spec *spec, s32 timeout);


bool ccm_childproc_fork(ccm_childproc *cp);
void ccm_childproc_read(ccm_childproc *cp);
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
    s32 j;
    c8 *compiler;
    c8 *output_flag;
    ccm_arena arena;
    ccm_str8_array common_opts;
    ccm_target_array deps;
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

void ccm_bootstrap(s32 argc, c8 **argv);

c8   *ccm_shift_args(s32 *argc, c8 ***argv);

void  ccm_cmd_print(ccm_arena a, c8 **cmd);



ccm_ring_buffer *ccm_compute_ready_queue(ccm_spec *spec);
void ccm_compute_dependents(ccm_spec *spec);


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
    ccm_log(CCM_LOG_INFO, "spec.arena total commited memory = %ld\n", arena->off);
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

// -----------------------------------------------------------------------------
// Ring Buffer
// -----------------------------------------------------------------------------
void ccm_rb_push(ccm_ring_buffer *rb, ccm_rbvalue_t v)
{
    ccm_assert(rb->len < rb->cap);
    ++rb->len;
    rb->items[rb->write] = v;
    rb->write = (rb->write + 1) % rb->cap;
}

ccm_rbvalue_t ccm_rb_pop(ccm_ring_buffer *rb)
{
    ccm_assert(rb->len > 0);
    --rb->len;
    ccm_rbvalue_t item = rb->items[rb->read];
    rb->read = (rb->read + 1)%rb->cap;
    return item;
}

ccm_rbvalue_t ccm_rb_peek(ccm_ring_buffer const *rb)
{
    return rb->len > 0 ? rb->items[rb->read] : NULL;
}

void ccm_rb_print(ccm_ring_buffer const *rb)
{
    ccm_log(CCM_LOG_DEBUG, "rb->cap = %d, rb->len = %d, rb->cap = %d, rb-> len = %d\n",
            rb->cap, rb->len, rb->read, rb->write);

    for (s32 i = 0; i < rb->len; ++i) {
        ccm_log(CCM_LOG_DEBUG, "rb[%d] = %s, ", i, rb->items[i + rb->read]->name);
    }
    ccm_log(CCM_LOG_NONE, "\n");
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

        if (execvp(pathname, (c8 *const*)argv) < 0) {
            ccm_log(CCM_LOG_ERROR, "execvp failed!\n");
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

void ccm_childproc_read(ccm_childproc *cp)
{
    c8 *buf = cp->report.items + cp->report.len;
    lll read_len = cp->report.cap - cp->report.len;
    lll n = 0;
    for (s32 i = 0; (n = read(cp->pipe.read, buf, read_len)) ; ++i) {
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EAGAIN) {
            break;
        }
        if (n == -1) {
            ccm_log(CCM_LOG_ERROR, "read: child %d failed: %s\n",
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

void ccm_childproc_report(ccm_childproc *cp)
{
    write(STDOUT_FILENO, cp->report.items, cp->report.len);
    /* reset the report buffer */
    memset(cp->report.items, 0, cp->report.len);
    cp->report.len = 0;
}


// -----------------------------------------------------------------------------
// ChildProc Manager
// -----------------------------------------------------------------------------
bool ccm_proc_mgr_add_target(ccm_proc_mgr *pm, ccm_target *t)
{
    if (pm->nrunning == pm->maxjobs) return false;
    s32 next_child = pm->nrunning;
    ccm_spec *spec = pm->spec;

    pm->evs[next_child] = 0;

    pm->cps[next_child].target = t;
    pm->cps[next_child].cmd = ccm_compile_cmd(spec, t);

    if (pipe2((int*)&pm->cps[next_child].pipe, O_NONBLOCK) < 0) {
        ccm_panic("ccm_proc_mgr_add_target: pipe2 failed, %s\n", strerror(errno));
    }

    pm->pfds[next_child].fd = pm->cps[next_child].pipe.read;
    pm->pfds[next_child].events = POLLIN;

    ++pm->nrunning;

    /* forks the child and sets up the pipe */
    ccm_childproc_fork(&pm->cps[next_child]);

    return true;
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

void ccm_proc_mgr_pub_ev(ccm_proc_mgr *pm)
{
    ccm_childproc *cps = pm->cps;
    pollfd *pfds       = pm->pfds;
    ccm_event *evs     = pm->evs;
    s32 nrunning       = pm->nrunning;

    s32 nready = poll(pfds, nrunning, pm->timeout);
    if (nready == -1) {
        ccm_panic("poll: polling bootstrap child proc failed with error %s\n",
                  strerror(errno));
    }

    ccm_assert(nready <= nrunning);

    for (s32 i = 0; nready && i < nrunning; ++i) {
        if (pfds[i].revents == 0) continue;

        if (pfds[i].revents & POLLIN) {
            evs[i] |= CCM_EVENT_POLLIN;
        }
        if (pfds[i].revents & POLLHUP) {
            evs[i] |= CCM_EVENT_POLLHUP;
            pfds[i].events = 0;
            pfds[i].revents = 0;
            pfds[i].fd = -1;
        }
        if (pfds[i].revents & POLLERR) {
            evs[i] |= CCM_EVENT_POLLERR;
        }

        --nready;
    }

    for (s32 i = 0; i < nrunning; ++i) {
        s32 ret = waitpid(cps[i].pid, &cps[i].status, WNOHANG);

        if (ret == -1) {
            if (errno == EINTR) evs[i] |= CCM_EVENT_WAIT_PENDING;
            if (errno == ECHILD) ccm_panic("Invalid child process\n");
            evs[i] |= CCM_EVENT_WAIT_ERROR;
            continue;
        }

        if (ret == 0) {
            evs[i] |= CCM_EVENT_WAIT_PENDING;
            continue;
        }

        if (WIFEXITED(cps[i].status)) {
            evs[i] |= CCM_EVENT_WAIT_DONE;
        } else if (WIFSIGNALED(cps[i].status)) {
            evs[i] |= CCM_EVENT_WAIT_TERM;
        }
    }
}

void ccm_proc_mgr_run(ccm_proc_mgr *pm)
{
    ccm_spec      *spec = pm->spec;
    ccm_childproc *cps  = pm->cps;
    pollfd        *pfds = pm->pfds;
    ccm_event     *evs  = pm->evs;


    s32 remaining_targets = pm->spec->deps.len;

    s32 read_mask = (CCM_EVENT_POLLIN | CCM_EVENT_POLLHUP);
    s32 done_mask = (CCM_EVENT_WAIT_DONE | CCM_EVENT_WAIT_ERROR |
                     CCM_EVENT_WAIT_TERM | CCM_EVENT_CLOSE_ERR);

    ccm_ring_buffer *ready_queue = ccm_compute_ready_queue(pm->spec);
    ccm_compute_dependents(pm->spec);

    while (remaining_targets > 0) {

        for (ccm_target *t = ccm_rb_peek(ready_queue);
             ready_queue->len > 0 && (ccm_target_needs_rebuild(t) ? ccm_proc_mgr_add_target(pm, t) : true);
             ccm_rb_pop(ready_queue), t = ccm_rb_peek(ready_queue)) {

        }

        /* reset events */
        for (s32 i = 0; i < pm->nrunning; ++i) evs[i] = 0;

        ccm_proc_mgr_pub_ev(pm);
        for (s32 i = 0; i < pm->nrunning; ++i) {

            if (evs[i] & read_mask) {
                ccm_childproc_read(&cps[i]);
                if (evs[i] & POLLHUP) {
                    if (close(cps[i].pipe.read) != 0) {
                        ccm_log(CCM_LOG_ERROR, "close: child %d failed: %s\n",
                                cps[i].pid, strerror(errno));
                    }
                }
            }

            if (evs[i] & done_mask) {
                /* update the ready queue with targets in current target depedent list */
                for (s32 d = 0; d < cps[i].target->revdeps.len; ++d) {
                    ccm_target *rt = cps[i].target->revdeps.items[d];
                    --rt->deps.len;
                    if (rt->deps.len == 0) {
                        ccm_rb_push(ready_queue, rt);
                    }
                }
                if (evs[i] & CCM_EVENT_WAIT_DONE) {
                    ccm_log(CCM_LOG_INFO, "job [%d] time: %f ms\n", cps[i].pid,
                            1000 * (double)(clock() - cps[i].time)/CLOCKS_PER_SEC);
                    ccm_as_scratch_arena(spec->arena) {
                        ccm_log(CCM_LOG_INFO, "CMD: %s\n",
                                ccm_concat(&spec->arena, cps[i].cmd));
                    }
                    ccm_childproc_report(&cps[i]);
                }
                ccm_swap(ccm_childproc, cps[i], cps[pm->nrunning - 1]);
                ccm_swap(pollfd, pfds[i], pfds[pm->nrunning - 1]);
                ccm_swap(ccm_event, evs[i], evs[pm->nrunning - 1]);
                --i;
                --pm->nrunning;
                --remaining_targets;
            }
            /* TODO: handle error events with proper error messages */
        }
    }
}

ccm_proc_mgr ccm_proc_mgr_init(ccm_spec *spec, s32 timeout)
{
    ccm_proc_mgr pm = {
        .maxjobs  = spec->j,
        .nrunning = 0,
        .timeout  = timeout,
        .spec = spec,
        .evs  = ccm_arena_alloc(ccm_event,     &spec->arena, spec->j),
        .cps  = ccm_arena_alloc(ccm_childproc, &spec->arena, spec->j),
        .pfds = ccm_arena_alloc(pollfd,        &spec->arena, spec->j),
    };

    for (s32 i = 0; i < pm.maxjobs; ++i) {
        ccm_da_init(&pm.cps[i].report, CCM_CHILDPROC_REPORT_BUF_CAP, CCM_ZERO_MEM);
    }

    return pm;
}

void ccm_proc_mgr_deinit(ccm_proc_mgr *pm)
{
    /* Note the order,
     * cps are allocated from the arena, so we must free them first
     */
    for (s32 i = 0; i < pm->maxjobs; ++i) ccm_da_deinit(&pm->cps[i].report);
}


#ifndef CCM_BOOTSTRAP_FLAGS
#define CCM_BOOTSTRAP_FLAGS                                     \
    "-Wall", "-Wextra", "-Werror", "-g0", "-O0", "-pipe",       \
    "-fsanitize=undefined,address,bounds"
#endif /* CCM_BOOTSTRAP_FLAGS */

#ifndef CCM_BOOTSTRAP_TIMEOUT
#define CCM_BOOTSTRAP_TIMEOUT 100
#endif /* CCM_BOOTSTRAP_TIMEOUT */

void ccm_bootstrap(s32 argc, c8 **argv)
{
    ccm_unused(argc);
    ccm_target bootstrap = {
        .name = "./ccm",
        .sources = ccm_str8_array("ccm.c", "ccm.h"),
    };
    if (!ccm_target_needs_rebuild(&bootstrap)) {
        ccm_log(CCM_LOG_INFO, "ccm upto date, skip bootstrapping\n");
        return;
    }

    ccm_spec spec = {
        .compiler = "cc",
        .output_flag = "-o",
        .common_opts = ccm_str8_array(CCM_BOOTSTRAP_FLAGS),
        .arena = ccm_arena_init(CCM_ARENA_DEFAULT_CAP),
        .deps = ccm_deps_array(&bootstrap),
        .j = 2,
    };

    c8 *oldname = ccm_fmt(&spec.arena, "%s.old", bootstrap.name);
    rename(bootstrap.name, oldname);

    ccm_proc_mgr pm = ccm_proc_mgr_init(&spec, CCM_BOOTSTRAP_TIMEOUT);
    {
        ccm_proc_mgr_run(&pm);
        if (WEXITSTATUS(pm.cps[0].status) == EXIT_FAILURE) {
            rename(oldname, bootstrap.name);
            exit(EXIT_FAILURE);
        }
    }
    ccm_proc_mgr_deinit(&pm);
    ccm_arena_deinit(&spec.arena);

    ccm_log(CCM_LOG_INFO, "bootstrap succeeded\n");
    execvp(bootstrap.name, argv);
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

#ifndef CCM_DEFAULT_TIMEOUT
#define CCM_DEFAULT_TIMEOUT 100
#endif

void ccm_spec_build(ccm_spec *spec)
{
    ccm_as_scratch_arena(spec->arena) {
        /* useful for cycle detection */
        ccm_target_array dfs = {0};
        dfs.items = ccm_arena_alloc(ccm_target*, &spec->arena, spec->deps.len);
        ccm_spec_schedule(spec, &dfs);
    }

    ccm_proc_mgr pm = ccm_proc_mgr_init(spec, CCM_DEFAULT_TIMEOUT);
    {
        /* this is where the ready-queue is populated and consumed */
        ccm_proc_mgr_run(&pm);
    }
    ccm_proc_mgr_deinit(&pm);

#ifdef CCM_STATS
    ccm_stats();
#endif  /* CCM_STATS */
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
