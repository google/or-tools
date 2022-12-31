load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@bazel_skylib//rules:copy_file.bzl", "copy_file")

copy_file(
    name = "config_h_generic",
    src = "src/config.h.generic",
    out = "src/config.h",
)

copy_file(
    name = "pcre2_h_generic",
    src = "src/pcre2.h.generic",
    out = "src/pcre2.h",
)

copy_file(
    name = "pcre2_chartables_c",
    src = "src/pcre2_chartables.c.dist",
    out = "src/pcre2_chartables.c",
)

cc_library(
    name = "pcre2",
    srcs = [
        "src/pcre2_auto_possess.c",
        "src/pcre2_compile.c",
        "src/pcre2_config.c",
        "src/pcre2_context.c",
        "src/pcre2_convert.c",
        "src/pcre2_dfa_match.c",
        "src/pcre2_error.c",
        "src/pcre2_extuni.c",
        "src/pcre2_find_bracket.c",
        "src/pcre2_maketables.c",
        "src/pcre2_match.c",
        "src/pcre2_match_data.c",
        "src/pcre2_newline.c",
        "src/pcre2_ord2utf.c",
        "src/pcre2_pattern_info.c",
        "src/pcre2_script_run.c",
        "src/pcre2_serialize.c",
        "src/pcre2_string_utils.c",
        "src/pcre2_study.c",
        "src/pcre2_substitute.c",
        "src/pcre2_substring.c",
        "src/pcre2_tables.c",
        "src/pcre2_ucd.c",
        "src/pcre2_ucptables.c",
        "src/pcre2_valid_utf.c",
        "src/pcre2_xclass.c",
        ":pcre2_chartables_c",
    ],
    hdrs = glob(["src/*.h"]) + [
        ":config_h_generic",
        ":pcre2_h_generic",
    ],
    defines = [
        "HAVE_CONFIG_H",
        "PCRE2_CODE_UNIT_WIDTH=8",
        "PCRE2_STATIC",
    ],
    includes = ["src"],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "pcre2demo",
    srcs = ["src/pcre2demo.c"],
    visibility = ["//visibility:public"],
    deps = [":pcre2"],
)
