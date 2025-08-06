#define CCM_STATS
#define CCM_IMPLEMENTATION
#include "ccm.h"

typedef void (*build_callable)(ccm_spec *);

void usage(c8 const* program)
{
    fprintf(stderr, "usage: %s <todo>\n", program);
    exit(1);
}

int main(i32 argc, c8 **argv)
{
    ccm_bootstrap("cc", argc, argv);
    build_callable bb = NULL;

    c8 *program = ccm_shift_args(&argc, &argv);
    if (argc < 1) {
        usage(program);
    } else {
        if (strcmp(argv[0], "build") == 0) bb = ccm_spec_build;
        else if (strcmp(argv[0], "clean") == 0) bb = ccm_spec_clean;
    }

#if 0
    ccm_spec b = {
        .compiler = "cc",
        .common_opts =
        ccm_string_array("-Wall", "-Wextra", "-g3", "-O2", "-fsanitize=address,bounds",
                     "-I."),
    };

    ccm_target triangle = {
        .name = "./bin/triangle",
        .sources = ccm_string_array("./examples/triangle.c"),
        .post_opts = ccm_string_array("-lraylib", "-lm"),
    };

    ccm_target obj2c = {
        .name = "./utils/obj2c",
        .sources = ccm_string_array("./utils/obj2c.c"),
    };

    ccm_target geometry = {
        .name = "./lib/geom",
        .sources = ccm_string_array("./lib/geometry.c"),
        .pre_opts = ccm_string_array("-c"),
    };

    ccm_target z_buffer = {
        .name = "./bin/z-buffer",
        .sources = ccm_string_array("./examples/z-buffer.c"),
        .deps = ccm_target_array(&geometry),
    };
    b.targets = ccm_target_array(&obj2c, &triangle, &z_buffer, &geometry);
    b.targets = ccm_target_array(&triangle);
    if (bb) bb(&b);
#endif
}

#if 0
int main()
{
    Build b = { .compiler = "cc",
                .default_flags = {"-Wall", "-Wextra",
                                  "-g3", "-O2" "-fsanitize=address,bounds"} };

    Executable obj2c = {
        .id = ID_OBJ2C,
        .name = "./utils/obj2c",
        .sources = {"./utils/obj2c.c", NULL},
        .compiler_opts = ,
    };

    Executable ppm_test = {
        .id = ID_PPM_TEST,
        .name = "./bin/ppm-test",
        .sources = {"./examples/ppm-test.c"},
    };

    Executable triangle = {
        .id = ID_TRIANGLE_EXAMPLE,
        .name = "./bin/triangle",
        .sources = {"./examples/triangle.c"},
        .compiler_opts = {"-lraylib", "-lm"}
    };

    Lib geometry = {
        .name = "./lib/geom",
        .type = BUILD_DYNAMIC_LIB | BUILD_STATIC_LIB,
        .sources = {"./lib/geometry.c"},
    };

    Executable z_buffer = {
        .name = "./bin/z-buffer",
        .sources = "./examples/z-buffer.c",
        .deps = { geometry },
    }
    /* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */
    Build_register_executable(b, obj2c);
    Build_register_executable(b, ppm_test);
    Build_register_executable(b, triangle);
    Build_register_executable(z_buffer);

    int *results = Build_buld_all(b);
    if (results[ID_OBJ2C]) {
        Build_run(obj2c, "./assets/file.obj", "./assets/file.c");
    }
    if (results[ID_PPM_TEST]) {
        Build_run(ppm_test);
    }
    if (results[ID_TRIANGLE_EXAMPLE]) {
        Build_run(triangle);
    }
}
#endif
