// BEGIN Ring Buffer Decl {
#define CCM_RINGBUFFER_INITIAL_CAP 64

typedef ccm_target* ccm_rbvalue_t;
typedef struct {
    s32 read;
    s32 write;
    s32 cap;
    s32 len;
    ccm_rbvalue_t items[];
} ccm_ring_buffer;

ccm_api ccm_ring_buffer* rb_init(ccm_arena *a, s32 cap);
ccm_api void rb_push(ccm_arena *a, ccm_ring_buffer *rb, ccm_rbvalue_t v);
ccm_api ccm_rbvalue_t rb_pop(ccm_ring_buffer *rb);
ccm_api ccm_rbvalue_t rb_peek(ccm_ring_buffer const *rb);
// } END Ring Buffer Decl

// BEGIN Ring Buffer Impl {
ccm_api ccm_ring_buffer* rb_init(ccm_arena *a, s32 cap)
{
    ccm_ring_buffer *rb =
        ccm_arena_flexalloc(ccm_ring_buffer, items, a, CCM_RINGBUFFER_INITIAL_CAP);
    rb->cap = cap;
    rb->len = 0;
    rb->write = 0;
    rb->read = 0;
    return rb;
}

ccm_api void rb_push(ccm_arena *a, ccm_ring_buffer *rb, ccm_rbvalue_t v)
{
    /* NOTE
    * This assumes ccm_arena a last allocation is still the ring buffer
    */
    if (rb->len >= rb->cap) {
        rb->cap = 2*rb->cap;
        ccm_arena_alloc(ccm_rbvalue_t, a, rb->cap);
    }
    ++rb->len;
    rb->items[rb->write] = v;
    rb->write = (rb->write + 1) % rb->cap;
}

ccm_api ccm_rbvalue_t rb_pop(ccm_ring_buffer *rb)
{
    if (rb->len == 0) {
        return NULL;
    }
    --rb->len;
    ccm_rbvalue_t item = rb->items[rb->read];
    rb->read = (rb->read + 1)%rb->cap;
    return item;
}

ccm_api ccm_rbvalue_t rb_peek(ccm_ring_buffer const *rb)
{
    if (rb->len == 0) {
        return NULL;
    }
    return rb->items[rb->read];
}
// } End Ring Buffer Impl
