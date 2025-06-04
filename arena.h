// BEGIN Arena Decl {
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

#ifndef CCM_ARENA_DEFAULT_CAP
#define CCM_ARENA_DEFAULT_CAP (64 * 1024 * 1024) /* 64mb */
#endif /* CCM_ARENA_DEFAULT_CAP */

typedef struct {
    u8 *memory;
    lll capacity;
    lll offset;
} ccm_arena;

ccm_api ccm_arena ccm_arena_init(lll capacity);
ccm_api void* ccm_arena_allocarray(ccm_arena *arena,
                                   lll itemsize, lll itemcount, lll align);
ccm_api void ccm_arena_reset(ccm_arena *arena);
ccm_api lll ccm_arena_save_mark(ccm_arena *arena);
ccm_api void ccm_arena_log_stats(ccm_arena *arena);
ccm_api void ccm_arena_deinit(ccm_arena *arena);

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

// } END Arena

// BEGIN Arena Impl {
ccm_api ccm_arena ccm_arena_init(lll capacity)
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

ccm_api void* ccm_arena_allocarray(ccm_arena *arena, lll itemsize, lll itemcount, lll align)
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

ccm_api void ccm_arena_reset(ccm_arena *arena)
{
    arena->offset = 0;
}

ccm_api lll ccm_arena_save_mark(ccm_arena *arena)
{
    return arena->offset;
}

ccm_api void ccm_arena_log_stats(ccm_arena *arena)
{
    ccm_log(CCM_INFO, "spec.arena total commited memory = %ld\n", arena->offset);
}

ccm_api void ccm_arena_deinit(ccm_arena *arena)
{
    ccm_arena_log_stats(arena);
    ccm_free(arena->memory);
    arena->capacity = 0;
    arena->offset = 0;
    arena->memory = NULL;
}
// } End Arena Impl
