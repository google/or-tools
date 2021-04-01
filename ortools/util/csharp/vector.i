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


%include "ortools/base/base.i"

%{
#include <vector>
#include "ortools/base/integral_types.h"
%}

// Typemaps to represent arguments of types:
// - "const std::vector<TYPE>&" or
// - "std::vector<TYPE>"
// as CSHARPTYPE[].
// note: TYPE must be a primitive data type (PDT).
%define VECTOR_AS_CSHARP_ARRAY(TYPE, CTYPE, CSHARPTYPE, ARRAYTYPE)
// This part is for const std::vector<>&.
%typemap(cstype) const std::vector<TYPE>& %{ CSHARPTYPE[] %}
%typemap(csin)   const std::vector<TYPE>& %{ $csinput.Length, $csinput %}
%typemap(imtype, out="global::System.IntPtr") const std::vector<TYPE>&  %{ int length$argnum, CSHARPTYPE[] %}
%typemap(ctype, out="void*") const std::vector<TYPE>&  %{ int length$argnum, CTYPE* %}
%typemap(in)     const std::vector<TYPE>&  %{
  $1 = new std::vector<TYPE>;
  $1->reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i) {
    $1->emplace_back($input[i]);
  }
%}
%typemap(freearg)  const std::vector<TYPE>&  { delete $1; }

%typemap(out) const std::vector<TYPE>& %{
  $result = new std::vector< CTYPE >((const std::vector< CTYPE> &)*$1);
%}
%typemap(csout, excode=SWIGEXCODE) const std::vector<TYPE>& {
  global::System.IntPtr cPtr = $imcall;$excode
  ARRAYTYPE tmpVector = null;
  if (cPtr != global::System.IntPtr.Zero) {
    tmpVector = new ARRAYTYPE(cPtr, true);
    CSHARPTYPE[] outArray = new CSHARPTYPE[tmpVector.Count];
    tmpVector.CopyTo(outArray);
    return outArray;
  }
  return null;
}
// Now, we do it for std::vector<>.
%typemap(cstype) std::vector<TYPE> %{ CSHARPTYPE[] %}
%typemap(csin)   std::vector<TYPE> %{ $csinput.Length, $csinput %}
%typemap(imtype, out="global::System.IntPtr") std::vector<TYPE>  %{ int length$argnum, CSHARPTYPE[] %}
%typemap(ctype, out="void*")   std::vector<TYPE>  %{ int length$argnum, CTYPE* %}
%typemap(in)      std::vector<TYPE>  %{
  $1.clear();
  $1.reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i) {
    $1.emplace_back($input[i]);
  }
%}

%typemap(out) std::vector<TYPE> %{
  $result = new std::vector< CTYPE >((const std::vector< CTYPE> &)$1);
%}
%typemap(csout, excode=SWIGEXCODE) std::vector<TYPE> {
  global::System.IntPtr cPtr = $imcall;$excode
  ARRAYTYPE tmpVector = null;
  if (cPtr != global::System.IntPtr.Zero) {
    tmpVector = new ARRAYTYPE(cPtr, true);
    CSHARPTYPE[] outArray = new CSHARPTYPE[tmpVector.Count];
    tmpVector.CopyTo(outArray);
    return outArray;
  }
  return null;
}
%enddef // VECTOR_AS_CSHARP_ARRAY

// Typemaps to represent arguments of types:
// - "const std::vector<std::vector<TYPE> >&" or
// - "std::vector<std::vector<TYPE> >*" or
// - "std::vector<std::vector<TYPE> >" or
// as CSHARPTYPE[][].
// note: TYPE must be a primitive data type (PDT).
%define JAGGED_MATRIX_AS_CSHARP_ARRAY(TYPE, CTYPE, CSHARPTYPE, ARRAYTYPE)
// This part is for const std::vector<std::vector<TYPE> >&.
%typemap(cstype) const std::vector<std::vector<TYPE> >&  %{ CSHARPTYPE[][] %}
%typemap(csin)   const std::vector<std::vector<TYPE> >&  %{
  $csinput.GetLength(0),
  NestedArrayHelper.GetArraySecondSize($csinput),
  NestedArrayHelper.GetFlatArray($csinput)
%}
%typemap(imtype, out="global::System.IntPtr") const std::vector<std::vector<TYPE> >&  %{
  int len$argnum##_1, int[] len$argnum##_2, CSHARPTYPE[]
%}
%typemap(ctype, out="void*")  const std::vector<std::vector<TYPE> >&  %{
  int len$argnum##_1, int len$argnum##_2[], CTYPE*
%}
%typemap(in) const std::vector<std::vector<TYPE> >&  (std::vector<std::vector<TYPE> > result) %{
  result.clear();
  result.resize(len$argnum##_1);

  TYPE* inner_array = reinterpret_cast<TYPE*>($input);
  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum##_1; ++index1) {
    result[index1].reserve(len$argnum##_2[index1]);
    for (int index2 = 0; index2 < len$argnum##_2[index1]; ++index2) {
      const TYPE value = inner_array[actualIndex];
      result[index1].emplace_back(value);
      actualIndex++;
    }
  }

  $1 = &result;
%}
// Now, we do it for std::vector<std::vector<TYPE> >*.
%typemap(cstype) std::vector<std::vector<TYPE> >*  %{ CSHARPTYPE[][] %}
%typemap(csin)   std::vector<std::vector<TYPE> >*  %{
  $csinput.GetLength(0),
  NestedArrayHelper.GetArraySecondSize($csinput),
  NestedArrayHelper.GetFlatArray($csinput)
%}
%typemap(imtype, out="global::System.IntPtr") std::vector<std::vector<TYPE> >*  %{
  int len$argnum##_1, int[] len$argnum##_2, CSHARPTYPE[]
%}
%typemap(ctype, out="void*")  std::vector<std::vector<TYPE> >*  %{
  int len$argnum##_1, int len$argnum##_2[], CTYPE*
%}
%typemap(in) std::vector<std::vector<TYPE> >*  (std::vector<std::vector<TYPE> > result) %{
  result.clear();
  result.resize(len$argnum##_1);

  TYPE* flat_array = reinterpret_cast<TYPE*>($input);
  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum##_1; ++index1) {
    result[index1].reserve(len$argnum##_2[index1]);
    for (int index2 = 0; index2 < len$argnum##_2[index1]; ++index2) {
      const TYPE value = flat_array[actualIndex];
      result[index1].emplace_back(value);
      actualIndex++;
    }
  }
  $1 = &result;
%}
// Now, we do it for std::vector<std::vector<TYPE> >.
%typemap(cstype) std::vector<std::vector<TYPE> >  %{ CSHARPTYPE[][] %}
%typemap(csin)   std::vector<std::vector<TYPE> >  %{
  $csinput.GetLength(0),
  NestedArrayHelper.GetArraySecondSize($csinput),
  NestedArrayHelper.GetFlatArray($csinput)
%}
%typemap(imtype, out="global::System.IntPtr") std::vector<std::vector<TYPE> >  %{
  int len$argnum##_1, int[] len$argnum##_2, CSHARPTYPE[]
%}
%typemap(ctype, out="void*")  std::vector<std::vector<TYPE> >  %{
  int len$argnum##_1, int len$argnum##_2[], CTYPE*
%}
%typemap(in) std::vector<std::vector<TYPE> > %{
  $1.clear();
  $1.resize(len$argnum##_1);

  TYPE* inner_array = reinterpret_cast<TYPE*>($input);
  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum##_1; ++index1) {
    $1[index1].reserve(len$argnum##_2[index1]);
    for (int index2 = 0; index2 < len$argnum##_2[index1]; ++index2) {
      const TYPE value = inner_array[actualIndex];
      $1[index1].emplace_back(value);
      actualIndex++;
    }
  }
%}
%enddef // JAGGED_MATRIX_AS_CSHARP_ARRAY

// Typemaps to represent arguments of types:
// - "const std::vector<std::vector<TYPE> >&" or
// - "std::vector<std::vector<TYPE> >*" or
// as CSHARPTYPE[,].
// note: TYPE must be a primitive data type (PDT).
%define REGULAR_MATRIX_AS_CSHARP_ARRAY(TYPE, CTYPE, CSHARPTYPE, ARRAYTYPE)
// This part is for const std::vector<std::vector<>>&.
%typemap(cstype) const std::vector<std::vector<TYPE> >&  %{ CSHARPTYPE[,] %}
%typemap(csin)   const std::vector<std::vector<TYPE> >&  %{
  $csinput.GetLength(0),
  $csinput.GetLength(1),
  NestedArrayHelper.GetFlatArrayFromMatrix($csinput)
%}
%typemap(imtype, out="global::System.IntPtr") const std::vector<std::vector<TYPE> >&  %{
  int len$argnum##_1, int len$argnum##_2, CSHARPTYPE[]
%}
%typemap(ctype, out="void*")  const std::vector<std::vector<TYPE> >&  %{
  int len$argnum##_1, int len$argnum##_2, CTYPE*
%}
%typemap(in) const std::vector<std::vector<TYPE> >&  (std::vector<std::vector<TYPE> > result) %{
  result.clear();
  result.resize(len$argnum##_1);

  TYPE* inner_array = reinterpret_cast<TYPE*>($input);
  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum##_1; ++index1) {
    result[index1].reserve(len$argnum##_2);
    for (int index2 = 0; index2 < len$argnum##_2; ++index2) {
      const TYPE value = inner_array[actualIndex];
      result[index1].emplace_back(value);
      actualIndex++;
    }
  }

  $1 = &result;
%}
// Now, we do it for std::vector<std::vector<>>*.
%typemap(cstype) std::vector<std::vector<TYPE> >*  %{ CSHARPTYPE[,] %}
%typemap(csin)   std::vector<std::vector<TYPE> >*  %{
  $csinput.GetLength(0),
  $csinput.GetLength(1),
  NestedArrayHelper.GetFlatArrayFromMatrix($csinput)
%}
%typemap(imtype, out="global::System.IntPtr") std::vector<std::vector<TYPE> >*  %{
  int len$argnum##_1, int len$argnum##_2, CSHARPTYPE[]
%}
%typemap(ctype, out="void*")  std::vector<std::vector<TYPE> >*  %{
  int len$argnum##_1, int len$argnum##_2, CTYPE*
%}
%typemap(in) std::vector<std::vector<TYPE> >*  (std::vector<std::vector<TYPE> > result) %{
  result.clear();
  result.resize(len$argnum##_1);

  TYPE* flat_array = reinterpret_cast<TYPE*>($input);
  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum##_1; ++index1) {
    result[index1].reserve(len$argnum##_2);
    for (int index2 = 0; index2 < len$argnum##_2; ++index2) {
      const TYPE value = flat_array[actualIndex];
      result[index1].emplace_back(value);
      actualIndex++;
    }
  }
  $1 = &result;
%}
%enddef // REGULAR_MATRIX_AS_CSHARP_ARRAY

// SWIG Macros to use std::vector<Type> and const std::vector<Type>& in .Net as
// regular .Net array, where Type is an integral numeric type.
// see: https://docs.microsoft.com/en-us/dotnet/csharp/language-reference/builtin-types/integral-numeric-types
// By default vector<vector<Type>> is mapped to a jagged array i.e. .Net type[][]
// If you want a regular matrix i.e. .Net type[,] use REGULAR_MATRIX_AS_CSHARP_ARRAY instead.
%include "std_vector.i"
%template(IntVector) std::vector<int>;
%template(IntVectorVector) std::vector<std::vector<int> >;
VECTOR_AS_CSHARP_ARRAY(int, int, int, IntVector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int, int, int, IntVectorVector);
//REGULAR_MATRIX_AS_CSHARP_ARRAY(int, int, int, IntVectorVector);

%template(Int64Vector) std::vector<int64_t>;
%template(Int64VectorVector) std::vector<std::vector<int64_t> >;
VECTOR_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64Vector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64VectorVector);
//REGULAR_MATRIX_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64VectorVector);
