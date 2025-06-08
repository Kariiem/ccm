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
    ccm_spec b = {
        .compiler = "cc",
        .output_flag = "-o",
        .common_opts = ccm_string_array("-Wall", "-Wextra", "-g3", "-O2",
                                        "-fsanitize=undefined,address,bounds"),
        .argv = argv,
        .argc = argc,
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
        .sources = ccm_string_array("./examples/triangle.c"),
        .linker_opts = ccm_string_array("-lraylib", "-lm"),
    };

    ccm_target obj2c = {
        .name = "./utils/obj2c",
        .sources = ccm_string_array("./utils/obj2c.c"),
    };

    ccm_target geometry = {
        .name = "./lib/geom",
        .sources = ccm_string_array("./lib/geometry.c"),
        .compiler_opts = ccm_string_array("-c"),
    };

    ccm_target z_buffer = {
        .name = "./bin/z-buffer",
        .sources = ccm_string_array("./examples/z-buffer.c"),
        /* .deps = ccm_deps_array(&geometry), */
    };

    /* triangle.deps = ccm_deps_array(&z_buffer, &geometry); */
    /* obj2c.deps = ccm_deps_array(&z_buffer, &geometry); */

    b.deps = ccm_deps_array(&obj2c, &triangle, &z_buffer, &geometry);


    if (bb) bb(&b);
}
