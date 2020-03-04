# SWIG Wrapper Generation
Using [swig](https://github.com/swig/swig) to generate wrapper it's easy thanks to the modern
[UseSWIG](https://cmake.org/cmake/help/latest/module/UseSWIG.html) module (**CMake >= 3.14**).

note: SWIG automatically put its target(s) in `all`, thus `cmake --build build` will also call
swig and generate `_module.so`.

# Policy
UseSWIG is impacted by two policies:
* [CMP0078](https://cmake.org/cmake/help/latest/policy/CMP0078.html):UseSWIG generates standard target names (CMake 3.13+).
* [CMP0086](https://cmake.org/cmake/help/latest/policy/CMP0086.html): UseSWIG honors `SWIG_MODULE_NAME` via `-module` flag (CMake 3.14+).

That's why I recommnend to use CMake >= 3.14 with both policies set to new for
SWIG development.

# int64_t Management
When working on a `k8` (aka `x86_64`) architecture, you may face issue with `int64_t`
and `uint64_t` management.

First `long long` and `long int` are **different** types and `int64_t` is just a
typedef on one of them...

## Linux
On Linux we have:
```
sizeof(long long): 8
sizeof(long int): 8
sizeof(int64_t): 8
```

### Gcc
First try to find where and how the compiler define `int64_t` and `uint64_t`.
```sh
grepc -rn "typedef.*int64_t;" /lib/gcc
/lib/gcc/x86_64-linux-gnu/9/include/stdint-gcc.h:43:typedef __INT64_TYPE__ int64_t;
/lib/gcc/x86_64-linux-gnu/9/include/stdint-gcc.h:55:typedef __UINT64_TYPE__ uint64_t;
```
So we need to find this compiler macro definition
```sh
gcc -dM -E -x c /dev/null | grep __INT64                 
#define __INT64_C(c) c ## L
#define __INT64_MAX__ 0x7fffffffffffffffL
#define __INT64_TYPE__ long int

gcc -dM -E -x c++ /dev/null | grep __INT64
#define __INT64_C(c) c ## L
#define __INT64_MAX__ 0x7fffffffffffffffL
#define __INT64_TYPE__ long int
```

### Clang
```sh
clang -dM -E -x c++ /dev/null | grep INT64_TYPE
#define __INT64_TYPE__ long int
#define __UINT64_TYPE__ long unsigned int
```

Clang, GNU compilers:  
`-dM` dumps a list of macros.  
`-E` prints results to stdout instead of a file.  
`-x c` and `-x c++` select the programming language when using a file without a filename extension, such as `/dev/null`

Ref: https://web.archive.org/web/20190803041507/http://nadeausoftware.com/articles/2011/12/c_c_tip_how_list_compiler_predefined_macros

## MacOS
On Catalina 10.15 we have:
```
sizeof(long long): 8
sizeof(long int): 8
sizeof(int64_t): 8
```

### Clang
```sh
clang -dM -E -x c++ /dev/null | grep INT64_TYPE
#define __INT64_TYPE__ long long int
#define __UINT64_TYPE__ long long unsigned int
```
with:
`-dM` dumps a list of macros.  
`-E` prints results to stdout instead of a file.  
`-x c` and `-x c++` select the programming language when using a file without a filename extension, such as `/dev/null`

Ref: https://web.archive.org/web/20190803041507/http://nadeausoftware.com/articles/2011/12/c_c_tip_how_list_compiler_predefined_macros

## Windows
Contrary to macOS and Linux, Windows 64bits (x86_64) try hard to keep compatibility, so we have:
```
sizeof(long int): 4
sizeof(long long): 8
sizeof(int64_t): 8
```

### Visual Studio 2019
Thus, in `stdint.h` we have:
```cpp
#if _VCRT_COMPILER_PREPROCESSOR

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
```

## SWIG int64_t management stuff
First, take a look at [Swig stdint.i](https://github.com/swig/swig/blob/3a329566f8ae6210a610012ecd60f6455229fe77/Lib/stdint.i#L20-L24).
So, when targeting Linux you **must use define SWIGWORDSIZE64** (i.e. `-DSWIGWORDSIZE64`) while
on macOS and Windows you **must not define it**.

Now the bad news, even if you can control the SWIG typedef using `SWIGWORDSIZE64`,
[SWIG Java](https://github.com/swig/swig/blob/3a329566f8ae6210a610012ecd60f6455229fe77/Lib/java/java.swg#L74-L77) and
[SWIG CSHARP](https://github.com/swig/swig/blob/1e36f51346d95f8b9848e682c2eb986e9cb9b4f4/Lib/csharp/csharp.swg#L117-L120) do not take it into account for typemaps...

So you may want to use this for Java:
```swig
#if defined(SWIGJAVA)
#if defined(SWIGWORDSIZE64)
%define PRIMITIVE_TYPEMAP(NEW_TYPE, TYPE)
%clear NEW_TYPE;
%clear NEW_TYPE *;
%clear NEW_TYPE &;
%clear const NEW_TYPE &;
%apply TYPE { NEW_TYPE };
%apply TYPE * { NEW_TYPE * };
%apply TYPE & { NEW_TYPE & };
%apply const TYPE & { const NEW_TYPE & };
%enddef // PRIMITIVE_TYPEMAP
PRIMITIVE_TYPEMAP(long int, long long);
PRIMITIVE_TYPEMAP(unsigned long int, long long);
#undef PRIMITIVE_TYPEMAP
#endif // defined(SWIGWORDSIZE64)
#endif // defined(SWIGJAVA)
```

and this for .Net:
```swig
#if defined(SWIGCSHARP)
#if defined(SWIGWORDSIZE64)
%define PRIMITIVE_TYPEMAP(NEW_TYPE, TYPE)
%clear NEW_TYPE;
%clear NEW_TYPE *;
%clear NEW_TYPE &;
%clear const NEW_TYPE &;
%apply TYPE { NEW_TYPE };
%apply TYPE * { NEW_TYPE * };
%apply TYPE & { NEW_TYPE & };
%apply const TYPE & { const NEW_TYPE & };
%enddef // PRIMITIVE_TYPEMAP
PRIMITIVE_TYPEMAP(long int, long long);
PRIMITIVE_TYPEMAP(unsigned long int, unsigned long long);
#undef PRIMITIVE_TYPEMAP
#endif // defined(SWIGWORDSIZE64)
#endif // defined(SWIGCSHARP)
```

So `int64_t` (i.e. `long int` in this case) will be correctly bind to Java/.Net primitive type `long`.

# swig_add_library()
You can use `OUTPUT_DIR` to change the output directory for the `.py` file e.g.:
```cmake
swig_add_library(pyFoo
  TYPE SHARED
  LANGUAGE python
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/python/${PROJECT_NAME}/Foo
  SOURCES foo.i)
```

# Doxygen
Since swig 4.0, swig can now [extract doxygen comments](http://www.swig.org/Doc4.0/Doxygen.html) from C++ to inject it in
Python and Java.

## Csharp documentation
note: Doxygen to csharp was [planned](https://github.com/swig/swig/wiki/SWIG-4.0-Development#doxygen-documentation) but currently is not supported.
