// Copyright 2010-2014 Google
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

// This file provides swig wrapping for some specialization of std::function
// parameters. Currently, swig does not support much of C++11 features, and
// especially not the std::function.

//
// Java callers will need to use a specific "type" of callbacks: they must
// specialize one of the existing generic callback classes defined in
// ../functions_swig_helpers.h (which are SWIG-wrapped to Java in a
// straightforward way). See the examples.

%include "ortools/base/base.i"

%include "std_common.i"
%include "std_string.i"

%{
#include <functional>
#include "ortools/base/integral_types.h"
#include "ortools/util/functions_swig_helpers.h"
%}


#define PARENTHIZE(ReturnType, Args...) ReturnType(Args)
#define CAT3(a, b, c) a ## b ## c

// The C preprocessor macros below use some tricks that make them work only if
// the actual C preprocessor expands them (not the SWIG preprocessor).
%{
// NAMES(int64, bool, int)  expands to:  , i0, i1, i2
#define NAMES_0
#define NAMES_1 i0
#define NAMES_2 i0, i1
#define NAMES_3 i0, i1, i2
#define NAMES(num) NAMES_ ## num

// INSERT_NAMES(int64, bool, int)  expands to:  int64 i0, bool i1, int i2
#define INSERT_NAMES_0()
#define INSERT_NAMES_1(arg0) arg0 i0
#define INSERT_NAMES_2(arg0, arg1) arg0 i0, arg1 i1
#define INSERT_NAMES_3(arg0, arg1, arg2) arg0 i0, arg1 i1, arg2 i2
#define INSERT_NAMES(num) INSERT_NAMES_ ## num

// Abbreviation of the java type corresponding to the given CType.
// Eg. JAVA_ABBREV(int64) expands to "J".
#define JAVA_ABBREV_int64 "J"
#define JAVA_ABBREV_int "I"
#define JAVA_ABBREV_bool "Z"
#define JAVA_ABBREV(x) JAVA_ABBREV_ ## x

// ABBREV(int64, bool, int64)  expands to:  JZJ
#define ABBREV_0()
#define ABBREV_1(arg1) JAVA_ABBREV(arg1)
#define ABBREV_2(arg1, arg2) JAVA_ABBREV(arg1) JAVA_ABBREV(arg2)
#define ABBREV_3(arg1, arg2, arg3) ABBREV_2(arg1, arg2) JAVA_ABBREV(arg3)
#define ABBREV(num) ABBREV_ ## num
%}

// See WRAP_STD_FUNCTION_JAVA below to understand why we need the __Unused__
// argument.
%define WRAP_STD_FUNCTION_JAVA_AUX(ClassPath, ClassName, CppClass,
                                   ReturnType, JavaReturnType, NumArgs,
                                   __Unused__, Args...)
// The macro expansions can be hard to follow, so we show an example of the
// expected macro expansion with: ReturnType=int64, Args=int64, bool.
// EXPANSION EXAMPLE: "int64(int64, bool)".
%typemap(in) std::function<PARENTHIZE(ReturnType, Args)> {
  jclass object_class = jenv->FindClass(ClassPath ClassName);
  if (nullptr == object_class) return $null;
  jmethodID method_id = jenv->GetStaticMethodID(
      object_class, "getCPtr", "(L" ClassPath ClassName ";)J");
  assert(method_id != nullptr);
  operations_research::swig_util::CppClass* const fun =
      reinterpret_cast<operations_research::swig_util::CppClass*>(
          jenv->CallStaticLongMethod(object_class, method_id, $input));
  // EXPANSION EXAMPLE: "int64 i0, bool i1".
  $1 = [fun](INSERT_NAMES(NumArgs)(Args))  {
    // EXPANSION EXAMPLE: "i0, i1".
    return fun->Run(NAMES(NumArgs));
  };
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) std::function<PARENTHIZE(ReturnType, Args)> "jobject"
%typemap(jtype) std::function<PARENTHIZE(ReturnType, Args)> ClassName
%typemap(jstype) std::function<PARENTHIZE(ReturnType, Args)> ClassName

// This typemap handles the conversion of the jtype to jstype typemap type
// and vice versa
%typemap(javain) std::function<PARENTHIZE(ReturnType, Args)> "$javainput"
%enddef

// NUM_ARGS_MINUS_1(Guard, 4, "Hello", -1) = 3; etc. This generic macro works
// with 1 to 4 args (variadic macros with a total of zero arguments don't work).
#define NUM_ARGS_MINUS_1(Args...) NUM_ARGS_AUX(Args, 3, 2, 1, 0)
#define NUM_ARGS_AUX(_1, _2, _3, _4, N, Args...) N

#define FIRST_ARG(x, Args...) x

// We make the wrapper even more convenient to use. See usage below.
//
// Note: the variadic 'Args...' actually contains the 'JavaReturnType' argument,
// then the (possibly empty) list of argument types of the function. We do this
// because despite what the SWIG documentation claims, variadic macros don't
// work well when their number of arguments is zero.
%define WRAP_STD_FUNCTION_JAVA(CppClass, Package, ReturnType, Args...)
WRAP_STD_FUNCTION_JAVA_AUX(Package, "CppClass", CppClass, ReturnType,
                           FIRST_ARG(Args), NUM_ARGS_MINUS_1(Args), Args)
%enddef

// For std::function<> involving void either as input or output type,
// the WRAP_STD_FUNCTION_JAVA macro doesn't work. So we use some
// custom code here.
// TODO(user): explain why.

//  --------- VoidToString ---------

%define WRAP_STD_FUNCTIONS_WITH_VOID_JAVA(ClassPath)
%typemap(in) std::function<std::string()> {
  jclass object_class =
    jenv->FindClass(ClassPath "VoidToString");
  if (nullptr == object_class) return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class, "getCPtr",
          "(L" ClassPath "VoidToString;)J");
  assert(method_id != nullptr);
  operations_research::swig_util::VoidToString* const fun =
      reinterpret_cast<operations_research::swig_util::VoidToString*>(
          jenv->CallStaticLongMethod(object_class, method_id, $input));
  $1 = [fun]() {
    return fun->Run();
  };
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) std::function<std::string()> "jobject"
%typemap(jtype) std::function<std::string()> "VoidToString"
%typemap(jstype) std::function<std::string()> "VoidToString"

// This typemap handles the conversion of the jstype to jtype typemap types
%typemap(javain) std::function<std::string()> "$javainput"

// --------- VoidToVoid ---------

%typemap(in) std::function<void()> {
  jclass object_class =
    jenv->FindClass(ClassPath "VoidToVoid");
  if (nullptr == object_class) return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class, "getCPtr",
          "(L" ClassPath "VoidToVoid;)J");
  assert(method_id != nullptr);
  operations_research::swig_util::VoidToVoid* const fun =
      reinterpret_cast<operations_research::swig_util::VoidToVoid*>(
          jenv->CallStaticLongMethod(object_class, method_id, $input));
  $1 = [fun]() { fun->Run(); };
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) std::function<void()> "jobject"
%typemap(jtype) std::function<void()> "VoidToVoid"
%typemap(jstype) std::function<void()> "VoidToVoid"

// This typemap handles the conversion of the jstype to jtype typemap types
%typemap(javain) std::function<void()> "$javainput"

// --------- LongToVoid ---------

%typemap(in) std::function<void(int64)> {
  jclass object_class =
    jenv->FindClass(ClassPath "LongToVoid");
  if (nullptr == object_class) return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class, "getCPtr",
          "(L" ClassPath "LongToVoid;)J");
  assert(method_id != nullptr);
  operations_research::swig_util::LongToVoid* const fun =
      reinterpret_cast<operations_research::swig_util::LongToVoid*>(
          jenv->CallStaticLongMethod(object_class, method_id, $input));
  $1 = [fun](int64 i) { fun->Run(i); };
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) std::function<void(int64)> "jobject"
%typemap(jtype) std::function<void(int64)> "LongToVoid"
%typemap(jstype) std::function<void(int64)> "LongToVoid"

// This typemap handles the conversion of the jstype to jtype typemap types
%typemap(javain) std::function<void(int64)> "$javainput"
%enddef  // WRAP_STD_FUNCTIONS_WITH_VOID_JAVA

%define WRAP_STD_FUNCTION_JAVA_CLASS_TO_VOID(CppClass, ClassPath, Parameter)
%typemap(in) std::function<void(operations_research::Parameter*)> {
  jclass object_class = jenv->FindClass(ClassPath "CppClass");
  if (nullptr == object_class) return $null;
  jmethodID method_id = jenv->GetStaticMethodID(
      object_class, "getCPtr", "(L" ClassPath "CppClass;)J");
  assert(method_id != nullptr);
  operations_research::swig_util::CppClass* const fun =
      reinterpret_cast<operations_research::swig_util::CppClass*>(
          jenv->CallStaticLongMethod(object_class, method_id, $input));
  $1 = [fun](operations_research::Parameter* param) { fun->Run(param); };
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) std::function<void(operations_research::Parameter*)> "jobject"
%typemap(jtype) std::function<void(operations_research::Parameter*)> "CppClass"
%typemap(jstype) std::function<void(operations_research::Parameter*)> "CppClass"

// This typemap handles the conversion of the jstype to jtype typemap types
%typemap(javain) std::function<void(operations_research::Parameter*)> "$javainput"

%feature("director") operations_research::swig_util::CppClass;
%rename (run) operations_research::swig_util::CppClass::Run;
%enddef  // WRAP_STD_FUNCTION_JAVA_CLASS_TO_VOID

// directors

%module(directors="1") operations_research_swig_util

// --------- Include the swig helpers file to create the director classes ------
// We cannot use %ignoreall/%unignoreall as this is not compatible with nested
// swig files.

%feature("director") operations_research::swig_util::LongToLong;
%rename (run) operations_research::swig_util::LongToLong::Run;
%feature("director") operations_research::swig_util::LongLongToLong;
%rename (run) operations_research::swig_util::LongLongToLong::Run;
%feature("director") operations_research::swig_util::IntIntToLong;
%rename (run) operations_research::swig_util::IntIntToLong::Run;
%feature("director") operations_research::swig_util::LongLongLongToLong;
%rename (run) operations_research::swig_util::LongLongLongToLong::Run;
%feature("director") operations_research::swig_util::LongToBoolean;
%rename (run) operations_research::swig_util::LongToBoolean::Run;
%feature("director") operations_research::swig_util::VoidToString;
%rename (run) operations_research::swig_util::VoidToString::Run;
%feature("director") operations_research::swig_util::VoidToBoolean;
%rename (run) operations_research::swig_util::VoidToBoolean::Run;
%feature("director") operations_research::swig_util::LongLongLongToBoolean;
%rename (run) operations_research::swig_util::LongLongLongToBoolean::Run;
%feature("director") operations_research::swig_util::LongToVoid;
%rename (run) operations_research::swig_util::LongToVoid::Run;
%feature("director") operations_research::swig_util::VoidToVoid;
%rename (run) operations_research::swig_util::VoidToVoid::Run;

%include "ortools/util/functions_swig_helpers.h"
