// BEGIN Logger Decl {
typedef enum {
    CCM_INFO,
    CCM_WARN,
    CCM_DEBUG,
    CCM_ERROR,
    CCM_LOG_LAST,
} ccm_log_level;

ccm_api void ccm_panic(c8 const *fmt, ...)  CCM_ATTR_PRINTF(1, 2);
ccm_api void ccm_log(ccm_log_level l, c8 const *fmt, ...)  CCM_ATTR_PRINTF(2, 3);
// } END Logger Decl

// BEGIN Logger Impl {
ccm_api void ccm_panic(c8 const *fmt, ...)
{
    fputs("[PANIC] :: ", stderr);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    abort();
}

ccm_api void ccm_log(ccm_log_level l, c8 const *fmt, ...)
{
    c8 const *level = NULL;
    va_list ap;
    switch(l) {
    case CCM_INFO:  level = "[INFO]";  break;
    case CCM_WARN:  level = "[WARN]";  break;
    case CCM_DEBUG: level = "[DEBUG]"; break;
    case CCM_ERROR: level = "[ERROR]"; break;
    default: ccm_unreachable();
    }

    fprintf(stderr, "%-10s ", level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
// } END Logger Impl
