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

// SWIG Macros to use std::vector<Num> and const std::vector<Num>& in Java,
// where Num is an atomic numeric type.
//
// Normally we'd simply use %include "std_vector.i" with the %template
// directive (see http://www.i.org/Doc1.3/Library.html#Library_nn15), but
// in google3 we can't, because exceptions are forbidden.
//
// TODO(user): move to base/swig/java.

%include "ortools/base/base.i"

%{
#include <vector>
#include "ortools/base/integral_types.h"
%}

// Typemaps to represent const std::vector<CType>& arguments as arrays of
// JavaType.
%define VECTOR_AS_JAVA_ARRAY(CType, JavaType, JavaTypeName)
%typemap(jni) const std::vector<CType>& "j" #JavaType "Array"
%typemap(jtype) const std::vector<CType>& #JavaType "[]"
%typemap(jstype) const std::vector<CType>& #JavaType "[]"
%typemap(javain) const std::vector<CType>& "$javainput"
%typemap(in) const std::vector<CType>& %{
  if($input) {
    $1 = new std::vector<CType>;
    const int size = jenv->GetArrayLength($input);
    $1->reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      JavaType value = values[i];
      $1->emplace_back(value);
    }
    jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, values, JNI_ABORT);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const std::vector<CType>& {
  delete $1;
}
%typemap(out) const std::vector<CType>& %{
  $result = jenv->New ## JavaTypeName ## Array($1->size());
  jenv->Set ## JavaTypeName ## ArrayRegion(
      $result, 0, $1->size(), reinterpret_cast<const j ## JavaType*>($1->data()));
%}
%typemap(javaout) const std::vector<CType>& {
  return $jnicall;
}

// Same, for std::vector<CType>
%typemap(jni) std::vector<CType> "j" #JavaType "Array"
%typemap(jtype) std::vector<CType> #JavaType "[]"
%typemap(jstype) std::vector<CType> #JavaType "[]"
%typemap(javain) std::vector<CType> "$javainput"
%typemap(in) std::vector<CType> %{
  if($input) {
    const int size = jenv->GetArrayLength($input);
    $1.clear();
    $1.reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      JavaType value = values[i];
      $1.emplace_back(value);
    }
    jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, values, JNI_ABORT);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(out) std::vector<CType> %{
  const std::vector<CType>& vec = $1;
  $result = jenv->New ## JavaTypeName ## Array(vec.size());
      jenv->Set ## JavaTypeName ## ArrayRegion($result, 0, vec.size(), reinterpret_cast<const j ## JavaType*>(vec.data()));
%}
%typemap(javaout) std::vector<CType> {
  return $jnicall;
}

%apply const std::vector<CType>& { const std::vector<CType>& }
%apply std::vector<CType> { std::vector<CType> }

%enddef  // VECTOR_AS_JAVA_ARRAY
VECTOR_AS_JAVA_ARRAY(int, int, Int);
VECTOR_AS_JAVA_ARRAY(int64, long, Long);
VECTOR_AS_JAVA_ARRAY(double, double, Double);

// Convert long[][] to std::vector<std::vector<int64>>
//
// TODO(user): move this code to a generic macro to convert
// std::vector<std::vector<T>> to T[][].
%typemap(jni) const std::vector<std::vector<int64> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(jstype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(javain) const std::vector<std::vector<int64> >& "$javainput"
%typemap(in) const std::vector<std::vector<int64> >& (std::vector<std::vector<int64> > result) {
  const int size = jenv->GetArrayLength($input);
  result.clear();
  result.resize(size);
  for (int index1 = 0; index1 < size; ++index1) {
    jlongArray inner_array =
        (jlongArray)jenv->GetObjectArrayElement($input, index1);
    const int tuple_size = jenv->GetArrayLength(inner_array);
    result[index1].reserve(tuple_size);
    jlong* const values =
        jenv->GetLongArrayElements((jlongArray)inner_array, NULL);
    for (int index2 = 0; index2 < tuple_size; ++index2) {
      const int64 value = values[index2];
      result[index1].emplace_back(value);
    }
    jenv->ReleaseLongArrayElements((jlongArray)inner_array, values, JNI_ABORT);
    jenv->DeleteLocalRef(inner_array);
  }
  $1 = &result;
}

%apply const std::vector<std::vector<int64> >& {
  const std::vector<std::vector<int64> >&
}
