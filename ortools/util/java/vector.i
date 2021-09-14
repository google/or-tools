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

// SWIG Macros to use std::vector<Num> and const std::vector<Num>& in Java,
// where Num is an atomic numeric type.
//

%include "stdint.i"

%include "ortools/base/base.i"

%{
#include <vector>
#include "ortools/base/integral_types.h"
%}

// Typemaps to represents arguments of types "const std::vector<CType>&" or
// "std::vector<CType>" as JavaType[] (<PrimitiveType>Array).
// note: CType must be a primitive data type (PDT).
// ref: https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Get_PrimitiveType_ArrayElements_routines
%define VECTOR_AS_JAVA_ARRAY(CType, JavaType, JavaTypeName)
// This part is for const std::vector<>&.
%typemap(jstype) const std::vector<CType>& #JavaType "[]"
%typemap(javain) const std::vector<CType>& "$javainput"
%typemap(jtype) const std::vector<CType>& #JavaType "[]"
%typemap(jni) const std::vector<CType>& "j" #JavaType "Array"
%typemap(in) const std::vector<CType>& %{
  if($input) {
    $1 = new std::vector<CType>;
    const int size = jenv->GetArrayLength($input);
    $1->reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      $1->emplace_back(values[i]);
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
// Now, we do it for std::vector<>.
%typemap(jstype) std::vector<CType> #JavaType "[]"
%typemap(javain) std::vector<CType> "$javainput"
%typemap(jtype) std::vector<CType> #JavaType "[]"
%typemap(jni) std::vector<CType> "j" #JavaType "Array"
%typemap(in) std::vector<CType> %{
  if($input) {
    const int size = jenv->GetArrayLength($input);
    $1.clear();
    $1.reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      $1.emplace_back(values[i]);
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
%enddef  // VECTOR_AS_JAVA_ARRAY

VECTOR_AS_JAVA_ARRAY(int, int, Int);
VECTOR_AS_JAVA_ARRAY(int64_t, long, Long);
VECTOR_AS_JAVA_ARRAY(double, double, Double);


// Typemaps to represents arguments of types "const std::vector<CType*>&" or
// "std::vector<CType*>" as JavaType[] (ObjectArray).
// note: CType is NOT a primitive type.
// note: CastOp defines how to cast the output of CallStaticLongMethod to CType*;
// its first argument is CType, its second is the output of CallStaticLongMethod.
// ref: https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#GetObjectArrayElement
%define CONVERT_VECTOR_WITH_CAST(CType, JavaType, CastOp, JavaPackage)
// This part is for const std::vector<CType*>&.
%typemap(jstype) const std::vector<CType*>& "JavaType[]"
%typemap(javain) const std::vector<CType*>& "$javainput"
%typemap(jtype) const std::vector<CType*>& "JavaType[]"
%typemap(jni) const std::vector<CType*>& "jobjectArray"
%typemap(in) const std::vector<CType*>& (std::vector<CType*> result) {
  std::string java_class_path = #JavaPackage "/" #JavaType;
  jclass object_class = jenv->FindClass(java_class_path.c_str());
  if (nullptr == object_class)
    return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(
          object_class, "getCPtr",
          std::string("(L" + java_class_path + ";)J").c_str());
  assert(method_id != nullptr);
  for (int i = 0; i < jenv->GetArrayLength($input); i++) {
    jobject elem = jenv->GetObjectArrayElement($input, i);
    jlong ptr_value = jenv->CallStaticLongMethod(object_class, method_id, elem);
    result.push_back(CastOp(CType, ptr_value));
  }
  $1 = &result;
}
%typemap(out) const std::vector<CType*>& {
  if (nullptr == $1)
    return $null;
  std::string java_class_path = #JavaPackage "/" #JavaType;
  jclass object_class = jenv->FindClass(java_class_path.c_str());
  if (nullptr == object_class)
    return $null;
  $result = jenv->NewObjectArray($1->size(), object_class, 0);
  jmethodID ctor = jenv->GetMethodID(object_class,"<init>", "(JZ)V");
  for (int i = 0; i < $1->size(); ++i) {
    jlong obj_ptr = 0;
    *((CType **)&obj_ptr) = (*$1)[i];
    jobject elem = jenv->NewObject(object_class, ctor, obj_ptr, false);
    jenv->SetObjectArrayElement($result, i, elem);
  }
}
%typemap(javaout) const std::vector<CType*> & {
  return $jnicall;
}
// Now, we do it for std::vector<CType*>.
%typemap(jstype) std::vector<CType*> "JavaType[]"
%typemap(javain) std::vector<CType*> "$javainput"
%typemap(jtype) std::vector<CType*> "JavaType[]"
%typemap(jni) std::vector<CType*> "jobjectArray"
%typemap(in) std::vector<CType*> (std::vector<CType*> result) {
  std::string java_class_path = #JavaPackage "/" #JavaType;
  jclass object_class = jenv->FindClass(java_class_path.c_str());
  if (nullptr == object_class)
    return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class,
                              "getCPtr",
                              std::string("(L" + java_class_path + ";)J").c_str());
  assert(method_id != nullptr);
  for (int i = 0; i < jenv->GetArrayLength($input); i++) {
    jobject elem = jenv->GetObjectArrayElement($input, i);
    jlong ptr_value = jenv->CallStaticLongMethod(object_class, method_id, elem);
    result.push_back(CastOp(CType, ptr_value));
  }
  $1 = result;
}
%enddef  // CONVERT_VECTOR_WITH_CAST


// Typemaps to represents arguments of types:
// "const std::vector<std::vector<CType>>&" or
// "std::vector<std::vector<CType>>*" or
// "std::vector<std::vector<CType>>" or
// as JavaType[][] (ObjectArray of JavaTypeArrays).
// note: CType must be a primitive data type (PDT).
// ref: https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#GetObjectArrayElement
// ref: https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Get_PrimitiveType_ArrayElements_routines
%define MATRIX_AS_JAVA_ARRAY(CType, JavaType, JavaTypeName)
// This part is for const std::vector<std::vector<CType> >&.
%typemap(jstype) const std::vector<std::vector<CType> >& #JavaType "[][]"
%typemap(javain) const std::vector<std::vector<CType> >& "$javainput"
%typemap(jtype) const std::vector<std::vector<CType> >& #JavaType "[][]"
%typemap(jni) const std::vector<std::vector<CType> >& "jobjectArray"
%typemap(in) const std::vector<std::vector<CType> >& (std::vector<std::vector<CType> > result) %{
  if($input) {
    const int size = jenv->GetArrayLength($input);
    result.clear();
    result.resize(size);
    for (int index1 = 0; index1 < size; ++index1) {
      j ## JavaType ## Array inner_array =
        (j ## JavaType ## Array)jenv->GetObjectArrayElement($input, index1);
      const int inner_size = jenv->GetArrayLength(inner_array);
      result[index1].reserve(inner_size);
      j ## JavaType * const values =
        jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)inner_array, NULL);
      for (int index2 = 0; index2 < inner_size; ++index2) {
        result[index1].emplace_back(values[index2]);
      }
      jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)inner_array, values, JNI_ABORT);
      jenv->DeleteLocalRef(inner_array);
    }
    $1 = &result;
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
// Now, we do it for std::vector<std::vector<CType> >*
%typemap(jstype) std::vector<std::vector<CType> >* #JavaType "[][]"
%typemap(javain) std::vector<std::vector<CType> >* "$javainput"
%typemap(jtype) std::vector<std::vector<CType> >* #JavaType "[][]"
%typemap(jni) std::vector<std::vector<CType> >* "jobjectArray"
%typemap(in) std::vector<std::vector<CType> >* (std::vector<std::vector<CType> > temp) %{
  if (!$input) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
    return $null;
  }
  $1 = &temp;
%}
%typemap(argout) std::vector<std::vector<CType> >* %{
  // Verify arg has enough inner array element(s) since we can't resize it.
  const int outer_size = $1->size();
  if (JCALL1(GetArrayLength, jenv, $input) < outer_size) {
    std::string message("Array must contain at least ");
    message += std::to_string(outer_size);
    message += " inner array element(s), only contains ";
    message += std::to_string(outer_size);
    message += " element(s).";
    SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, message.c_str());
    return $null;
  }

  for (int index1 = 0; index1 < outer_size; ++index1) {
    // Create inner array
    const int inner_size = (*$1)[index1].size();
    j##JavaType##Array inner_array = JCALL1(New##JavaTypeName##Array, jenv, inner_size);
    // Copy data in it
    JCALL4(Set##JavaTypeName##ArrayRegion, jenv,
      inner_array,
      0,
      inner_size,
      reinterpret_cast<const j##JavaType*>((*$1)[index1].data()));
    // Add innner_array to $input
    JCALL3(SetObjectArrayElement, jenv, $input, index1, inner_array);
  }
%}
// Now, we do it for std::vector<std::vector<CType> >
%typemap(jstype) std::vector<std::vector<CType> > #JavaType "[][]"
%typemap(javain) std::vector<std::vector<CType> > "$javainput"
%typemap(jtype) std::vector<std::vector<CType> > #JavaType "[][]"
%typemap(jni) std::vector<std::vector<CType> > "jobjectArray"
%typemap(in) std::vector<std::vector<CType> > %{
  if($input) {
    const int size = jenv->GetArrayLength($input);
    $1.clear();
    $1.resize(size);
    for (int index1 = 0; index1 < size; ++index1) {
      j ## JavaType ## Array inner_array =
        (j ## JavaType ## Array)jenv->GetObjectArrayElement($input, index1);
      const int inner_size = jenv->GetArrayLength(inner_array);
      $1[index1].reserve(inner_size);
      j ## JavaType * const values =
        jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)inner_array, NULL);
      for (int index2 = 0; index2 < inner_size; ++index2) {
        $1[index1].emplace_back(values[index2]);
      }
      jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)inner_array, values, JNI_ABORT);
      jenv->DeleteLocalRef(inner_array);
    }
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%enddef  // MATRIX_AS_JAVA_ARRAY

MATRIX_AS_JAVA_ARRAY(int, int, Int);
MATRIX_AS_JAVA_ARRAY(int64_t, long, Long);
MATRIX_AS_JAVA_ARRAY(double, double, Double);

%define REINTERPRET_CAST(CType, ptr)
reinterpret_cast<CType*>(ptr)
%enddef
