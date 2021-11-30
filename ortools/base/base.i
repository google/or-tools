// Copyright 2010-2021 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

%{
#include <cstdint>
#include <string>
#include <vector>

#include "ortools/base/basictypes.h"
%}

%include "typemaps.i"
%include "stdint.i"
%include "std_string.i"

#ifdef SWIGPYTHON

#pragma SWIG nowarn=312,451,454,503,362
// 312 suppresses warnings about nested classes that SWIG doesn't currently
// support.
// 451 suppresses warnings about setting const char * variable may leak memory.
// 454 suppresses setting global ptr/ref variables may leak memory warning
// 503 suppresses warnings about identifiers that SWIG can't wrap without a
// rename.  For example, an operator< in a class without a rename.
// 362 is similar to 503 but for operator=.
%{
#include "ortools/base/python-swig.h"
%}

%include "exception.i"

#endif // SWIGPYTHON


#if defined(SWIGJAVA)
// swig/java/typenames.i and swig/java/java.swg typemap C++ 'long int' as Java 'int'
// but in C++ 'int64_t' aka 'int64_t' is defined as "long int" and we have
// overload functions int/int64_t in routing...
// So we need to force C++ 'long int' to map to Java 'long' instead of 'int' reusing the
// typemap for C++ `long long`
// note: there is no `ulong` in java so we map both on Java `long` type.
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


#if defined(SWIGCSHARP)
// same issue in csharp see csharp/typenames.i and csharp/csharp.swg
// note: csharp provide `long` and `ulong` primitive types.
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

// SWIG macros for explicit API declaration.
// Usage:
//
// %ignoreall
// %unignore SomeName;   // namespace / class / method
// %include "somelib.h"
// %unignoreall  // mandatory closing "bracket"
%define %ignoreall %ignore ""; %enddef
%define %unignore %rename("%s") %enddef
%define %unignoreall %rename("%s") ""; %enddef
