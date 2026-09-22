#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generate copts for OR-Tools.

This is the source of truth for OR-Tools compiler options.  To modify OR-Tools
compilation options:

  (1) Edit the appropriate list in this file based on the platform the flag is
      needed on.
  (2) Run `bazel run //ortools/copts:generate_copts`.

The generated copts are consumed by configure_copts.bzl and
ORToolsConfigureCopts.cmake.
"""

# pylint: disable=line-too-long

################################################################################
# GCC
################################################################################

ORTOOLS_GCC_FLAGS = [
    # go/keep-sorted start prefix_order="-D,"-f,"-Wno-,"-W"
    "-DNOMINMAX",  # Don't define min and max macros (Build on Windows using gcc)
    "-ffp-contract=off",  # Disable automatic fused-multiply-add insertion
    "-Wno-sign-compare",  # Disables warnings when comparing signed and unsigned values because of internal Style Guide.
    "-Wall",  # Enables a core set of warnings about questionable constructs
    "-Wcast-qual",  # Warns when a pointer is cast to remove a type qualifier (e.g., const)
    "-Wconversion-null",  # Warns for conversions between NULL and non-pointer types
    "-Wextra",  # Enables extra warning flags that are not enabled by -Wall
    "-Wformat-security",  # Warns about uses of format functions that represent possible security problems
    "-Wmissing-declarations",  # Warns if a global function is defined without a previous declaration
    "-Wnon-virtual-dtor",  # Warns when a class has virtual functions and an accessible non-virtual destructor
    "-Woverlength-strings",  # Warns about string literals longer than the maximum length specified by the standard
    "-Wpointer-arith",  # Warns about anything that depends on the "size of" a function type or of void
    "-Wundef",  # Warns if an undefined identifier is evaluated in an #if directive
    "-Wunused-local-typedefs",  # Warns when typedefs locally defined in a function are not used
    "-Wunused-result",  # Warns if a caller of a function marked with warn_unused_result/[[nodiscard]] does not use its return value
    "-Wvarargs",  # Warns for questionable usage of the macros used to retrieve variable arguments
    "-Wvla",  # Warns if a variable-length array is used
    "-Wwrite-strings",  # Gives string constants the type const char[length] so copying their address into a non-const char* produces a warning
    # go/keep-sorted end
] + [
    # This is a burndown list that will eventually disappear, it helps in
    # keeping the warnings low on the CI but they all need to be either removed
    # or adopted as an official flag.
    # go/keep-sorted start prefix_order="-D,"-f,"-Wno-,"-W"
    "-Wno-cast-qual",  # Disables warnings when a pointer is cast to remove a type qualifier (e.g., const)
    "-Wno-comment",  # Disables warnings for comments that contain non-ASCII characters
    "-Wno-deprecated-declarations",  # Disables warnings about uses of functions, variables, or types marked as deprecated
    "-Wno-float-conversion",  # Disables warnings for implicit conversions that reduce floating-point precision
    "-Wno-format-security",  # Disables warnings about uses of format functions that represent possible security problems
    "-Wno-ignored-qualifiers",  # Disables warnings when a qualifier is applied to a type that has no effect
    "-Wno-implicit-fallthrough",  # Disables warnings for implicit fallthrough in switch statements
    "-Wno-missing-declarations",  # Disables warnings if a global function is defined without a previous declaration
    "-Wno-missing-field-initializers",  # Disables warnings when a class has uninitialized members
    "-Wno-range-loop-construct",  # Disables warnings when a range-based for loop is used with a non-range type
    "-Wno-redundant-move",  # Disables warnings when a value is moved to itself with std::move
    "-Wno-sign-compare",  # Disables warnings when comparing signed and unsigned values
    "-Wno-sign-conversion",  # Disables warnings for implicit conversions that may change the sign of an integer value
    "-Wno-type-limits",  # Disables warnings when a type is too small to hold a value
    "-Wno-undef",  # Disables warnings when an undefined identifier is evaluated in an #if directive
    "-Wno-unused-but-set-variable",  # Disables warnings when a variable is assigned but never used
    "-Wno-unused-parameter",  # Disables warnings whenever a function parameter is unused aside from its declaration
    "-Wno-unused-result",  # Disables warnings when the result of a function is unused
    "-Wno-unused-variable",  # Disables warnings when a variable is declared but never used
    # go/keep-sorted end
]

ORTOOLS_GCC_TEST_ADDITIONAL_FLAGS = [
    # go/keep-sorted start prefix_order="-D,"-f,"-Wno-,"-W"
    "-Wno-deprecated-declarations",  # Disables warnings about uses of functions, variables, or types marked as deprecated
    "-Wno-missing-declarations",  # Disables warnings if a global function is defined without a previous declaration
    "-Wno-self-move",  # Disables warnings when a value is moved to itself with std::move
    "-Wno-unused-function",  # Disables warnings whenever a static or inline function is declared but not used
    "-Wno-unused-parameter",  # Disables warnings whenever a function parameter is unused aside from its declaration
    "-Wno-unused-private-field",  # Disables warnings when a private class field is declared but never used
    # go/keep-sorted end
]


################################################################################
# Clang based build for Linux, Macos and Windows.
################################################################################

# https://github.com/llvm/llvm-project/issues/102982
# A list of LLVM base flags without -Wall. This is because clang-cl
# translates -Wall to -Weverything on Windows, mimicking MSVCs
# behavior. On most other platforms, -Wall is just a set of very good
# default flags.
ORTOOLS_LLVM_BASE_FLAGS = [
    # go/keep-sorted start prefix_order="-D,"-f,"-Wno-,"-W"
    "-DNOMINMAX",  # Don't define min and max macros (Build on Windows using clang)
    "-ffp-contract=off",  # Disable automatic fused-multiply-add insertion
    "-Wno-float-conversion",  # Warnings that are enabled by group warning flags like -Wall that we explicitly disable.
    "-Wno-implicit-float-conversion",  # Disables warnings for implicit conversions that reduce floating-point precision or convert float to integer
    "-Wno-implicit-int-float-conversion",  # Disables warnings for implicit conversions from integer types to floating-point types that may lose precision
    "-Wno-sign-compare",  # Disables warnings when comparing signed and unsigned values because of internal Style Guide.
    "-Wno-unknown-warning-option",  # Disable warnings on unknown warning flags (when warning flags are unknown on older compiler versions)
    "-Wno-unused-command-line-argument",  # Disables warnings when a command-line argument is not used by the compiler
    "-Wc++98-compat-extra-semi",  # Warns about redundant semicolons outside of a function (incompatible with C++98)
    "-Wcast-qual",  # Warns when a pointer is cast to remove a type qualifier (e.g., const)
    "-Wconversion",  # Warns for implicit conversions that may alter a value
    "-Wdeprecated-pragma",  # Warns about deprecated pragma directives
    "-Wextra",  # Enables extra warning flags that are not enabled by -Wall or -Wmost
    "-Wfloat-overflow-conversion",  # Warns when a floating-point value is implicitly converted to an integer type and overflows
    "-Wfloat-zero-conversion",  # Warns when a non-zero floating-point value is implicitly converted to a zero integer value
    "-Wfor-loop-analysis",  # Warns about suspicious variable usage in for-loop headers
    "-Wformat-security",  # Warns about uses of format functions that represent possible security problems
    "-Wgnu-redeclared-enum",  # Warns about GNU extension allowing redeclaration of an already-declared enum
    "-Winfinite-recursion",  # Warns when a function appears to call itself infinitely on all code paths
    "-Winvalid-constexpr",  # Warns when a constexpr function or constructor can never produce a constant expression
    "-Wliteral-conversion",  # Warns when a literal is implicitly converted to a type that cannot preserve its value
    "-Wmissing-declarations",  # Warns if a global function is defined without a previous declaration
    "-Wmost",  # Enables most warnings (subset of -Wall)
    "-Wnullability-completeness",  # Warns when nullability annotations (_Nullable, _Nonnull, etc.) are incomplete in a file that uses them
    "-Woverlength-strings",  # Warns about string literals longer than the maximum length specified by the standard
    "-Wpointer-arith",  # Warns about anything that depends on the "size of" a function type or of void
    "-Wself-assign",  # Warns when a variable is assigned to itself
    "-Wshadow-all",  # Warns whenever a declaration shadows another variable, parameter, field, or global
    "-Wshorten-64-to-32",  # Warns when a 64-bit value is implicitly converted to a 32-bit type
    "-Wsign-conversion",  # Warns for implicit conversions that may change the sign of an integer value
    "-Wstring-conversion",  # Warns for implicit conversions from string literals to bool
    "-Wtautological-overlap-compare",  # Warns about compound comparisons with overlapping or impossible ranges
    "-Wtautological-unsigned-zero-compare",  # Warns when an unsigned value is compared against zero in a tautological way (e.g., < 0 or >= 0)
    "-Wthread-safety",  # Enables static thread-safety analysis based on lock annotations
    "-Wundef",  # Warns if an undefined identifier is evaluated in an #if directive
    "-Wuninitialized",  # Warns if an automatic variable is used without first being initialized
    "-Wunreachable-code",  # Warns when the compiler detects code that will never be executed
    "-Wunused-comparison",  # Warns when the result of a comparison is unused
    "-Wunused-local-typedefs",  # Warns when typedefs locally defined in a function are not used
    "-Wunused-result",  # Warns if a caller of a function marked with warn_unused_result/[[nodiscard]] does not use its return value
    "-Wvla",  # Warns if a variable-length array is used
    "-Wwrite-strings",  # Gives string constants the type const char[length] so copying their address into a non-const char* produces a warning
    # go/keep-sorted end
]

ORTOOLS_LLVM_FLAGS = [
    "-Wall",  # Enables a core set of warnings about questionable constructs
] + ORTOOLS_LLVM_BASE_FLAGS

ORTOOLS_LLVM_TEST_ADDITIONAL_FLAGS = [
    # go/keep-sorted start prefix_order="-D,"-f,"-Wno-,"-W"
    "-Wno-deprecated-declarations",  # Disables warnings about uses of functions, variables, or types marked as deprecated
    "-Wno-gnu-zero-variadic-macro-arguments",  # gtest depends on this GNU extension being offered.
    "-Wno-implicit-int-conversion",  # Disables warnings for implicit integer conversions that reduce precision
    "-Wno-missing-prototypes",  # Disables warnings if a global function is defined without a previous prototype declaration
    "-Wno-missing-variable-declarations",  # Disables warnings if a global variable is defined without a previous extern declaration
    "-Wno-nullability-completeness",  # Disables warnings about incomplete nullability annotations
    "-Wno-shadow",  # Disables warnings when a declaration shadows another variable or declaration
    "-Wno-shorten-64-to-32",  # Disables warnings when a 64-bit value is implicitly converted to a 32-bit type
    "-Wno-sign-compare",  # Disables warnings when comparing signed and unsigned values
    "-Wno-sign-conversion",  # Disables warnings for implicit conversions that may change the sign of an integer value
    "-Wno-unreachable-code-loop-increment",  # Disables warnings when a loop increment expression is unreachable
    "-Wno-unused-function",  # Disables warnings whenever a static or inline function is declared but not used
    "-Wno-unused-member-function",  # Disables warnings when an internal/private member function is unused
    "-Wno-unused-parameter",  # Disables warnings whenever a function parameter is unused aside from its declaration
    "-Wno-unused-private-field",  # Disables warnings when a private class field is declared but never used
    "-Wno-unused-template",  # Disables warnings when an internal/unexported template is unused
    "-Wno-used-but-marked-unused",  # Disables warnings when a symbol marked with the unused attribute is actually used
    # go/keep-sorted end
]

################################################################################
# MSVC
################################################################################

MSVC_BIG_WARNING_FLAGS = [
    "/W3",  # /Wall with msvc includes unhelpful warnings such as C4711, C4710, ...
]

MSVC_BASE_FLAGS = [
    # go/keep-sorted start
    "/Zc:preprocessor",  # Enable preprocessor conformance mode needed to correctly support __VA_OPT__
    "/bigobj",  # Increase the number of sections available in object files
    # go/keep-sorted end
]

MSVC_WARNING_FLAGS = [
    # go/keep-sorted start numeric=yes
    "/wd4005",  # macro-redefinition
    "/wd4018",  # signed/unsigned relational comparisons because of internal Style Guide.
    "/wd4068",  # unknown pragma
    "/wd4180",  # qualifier applied to function type has no meaning; ignored
    "/wd4389",  # signed/unsigned equality comparisons because of internal Style Guide.
    "/wd4503",  # The decorated name was longer than the compiler limit
    "/wd4800",  # forcing value to bool 'true' or 'false' (performance warning)
    # go/keep-sorted end
]

MSVC_DEFINES = [
    # go/keep-sorted start
    "/DNOMINMAX",  # Don't define min and max macros (windows.h)
    "/DWIN32_LEAN_AND_MEAN",  # Don't bloat namespace with incompatible winsock versions.
    "/D_CRT_SECURE_NO_WARNINGS",  # Don't warn about usage of insecure C functions.
    "/D_ENABLE_EXTENDED_ALIGNED_STORAGE",  # Introduced in VS 2017 15.8, allow overaligned types in aligned_storage
    "/D_SCL_SECURE_NO_WARNINGS",  # Don't warn about usage of insecure C functions.
    # go/keep-sorted end
]

MSVC_TEST_ADDITIONAL_FLAGS = [
    # go/keep-sorted start numeric=yes
    "/wd4101",  # unreferenced local variable
    "/wd4244",  # shortening conversion
    "/wd4267",  # shortening conversion
    "/wd4503",  # decorated name length exceeded, name was truncated
    "/wd4996",  # use of deprecated symbol
    # go/keep-sorted end
]

MSVC_LINKOPTS = [
    "-ignore:4221",  # Object file doesn't export any previously undefined symbols
]


def _gcc_style_filter_and_combine(default_flags, test_flags):
    """Merges default_flags and test_flags for GCC and LLVM.

    Args:
      default_flags: A list of default compiler flags
      test_flags: A list of flags that are only used in tests

    Returns:
      A combined list of default_flags and test_flags, but with all flags of the
      form '-Wwarning' removed if test_flags contains a flag of the form
      '-Wno-warning'
    """
    remove = set()
    for f in test_flags:
        without_prefix = f.removeprefix("-Wno-")
        if without_prefix != f:
            remove.add(f"-W{without_prefix}")
    return [f for f in default_flags if f not in remove] + test_flags


COPT_VARS = {
    "gcc": {
        "flags": ORTOOLS_GCC_FLAGS,
        "test_flags": _gcc_style_filter_and_combine(
            ORTOOLS_GCC_FLAGS, ORTOOLS_GCC_TEST_ADDITIONAL_FLAGS
        ),
        "linkopts": [],
    },
    "llvm": {
        "flags": ORTOOLS_LLVM_FLAGS,
        "test_flags": _gcc_style_filter_and_combine(
            ORTOOLS_LLVM_FLAGS, ORTOOLS_LLVM_TEST_ADDITIONAL_FLAGS
        ),
        "linkopts": [],
    },
    "clang_cl": {
        "flags": (MSVC_BIG_WARNING_FLAGS + MSVC_DEFINES + ORTOOLS_LLVM_BASE_FLAGS),
        "test_flags": (
            MSVC_BIG_WARNING_FLAGS
            + MSVC_DEFINES
            + _gcc_style_filter_and_combine(
                ORTOOLS_LLVM_BASE_FLAGS, ORTOOLS_LLVM_TEST_ADDITIONAL_FLAGS
            )
        ),
        "linkopts": [],
    },
    "msvc": {
        "flags": (
            MSVC_BASE_FLAGS + MSVC_BIG_WARNING_FLAGS + MSVC_WARNING_FLAGS + MSVC_DEFINES
        ),
        "test_flags": (
            MSVC_BASE_FLAGS
            + MSVC_BIG_WARNING_FLAGS
            + MSVC_WARNING_FLAGS
            + MSVC_DEFINES
            + MSVC_TEST_ADDITIONAL_FLAGS
        ),
        "linkopts": MSVC_LINKOPTS,
    },
}
