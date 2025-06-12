#define CCM_IMPLEMENTATION
#include "ccm.h"

#define ignore(v) ((void)(v))
typedef void (*build_callable)(ccm_spec *);

void usage(c8 const* program)
{
    fprintf(stderr, "usage: %s <todo>\n", program);
    exit(1);
}

int main(s32 argc, c8 **argv)
{
    ccm_bootstrap(argc, argv);

    ccm_spec b = {
        .compiler = "cc",
        .output_flag = "-o",
        .arena = ccm_arena_init(CCM_ARENA_DEFAULT_CAP),
        .common_opts = ccm_str8_array("-Wall",
                                      "-Wextra",
                                      "-g3",
                                      "-fsanitize=undefined,address,bounds"),
        .j = 3,
    };

    c8 *program = ccm_shift_args(&argc, &argv);
    build_callable bb = NULL;
    if (argc < 1) {
        usage(program);
    } else {
        if (strcmp(argv[0], "build") == 0) bb = ccm_spec_build;
        else if (strcmp(argv[0], "clean") == 0) bb = ccm_spec_clean;
    }

    ccm_target triangle = {
        .name = "./bin/triangle",
        .sources = ccm_str8_array("./examples/triangle.c"),
        .linker_opts = ccm_str8_array("-lraylib", "-lm"),
    };

    ccm_target obj2c = {
        .name = "./utils/obj2c",
        .sources = ccm_str8_array("./utils/obj2c.c"),
    };

    ccm_target geometry = {
        .name = "./lib/geom",
        .sources = ccm_str8_array("./lib/geometry.c"),
        .compiler_opts = ccm_str8_array("-c"),
    };

    ccm_target z_buffer = {
        .name = "./bin/z-buffer",
        .sources = ccm_str8_array("./examples/z-buffer.c"),
        /* .deps = ccm_deps_array(&geometry), */
    };

    /* triangle.deps = ccm_deps_array(&z_buffer, &geometry); */
    /* obj2c.deps = ccm_deps_array(&z_buffer, &geometry); */
    ccm_ignore(((void*[]){ &z_buffer, &geometry}));

    b.deps = ccm_deps_array(&obj2c, &triangle);


    if (bb) bb(&b);

    ccm_arena_deinit(&b.arena);
}
