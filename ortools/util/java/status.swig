// Copyright 2010-2025 Google LLC
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

// Adds support for absl::StatusOr<T>.
%include "ortools/base/base.i"

%{
#include "absl/status/statusor.h"
%}

%define STATUSOR_RETURN(CType)
%typemap(jni) absl::StatusOr<CType> "$typemap(jni, CType)"
%typemap(jtype) absl::StatusOr<CType> "$typemap(jtype, CType)"
%typemap(jstype) absl::StatusOr<CType> "$typemap(jstype, CType)"
%typemap(javaout) absl::StatusOr<CType> "$typemap(javaout, CType)"
%typemap(out) absl::StatusOr<CType> {
  if (!$1.ok()) {
    SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException,
                            $1.status().ToString().c_str());
    return $null;
  }
  // Use a lambda to define a local scope where we can use $1 and $result with
  // the typemaps of CType.
  $typemap(jni, CType) jval = [jenv](CType cppval) -> $typemap(jni, CType) {
    SwigValueWrapper<CType > $1;
    // Note that there is only `SwigValueWrapper& operator=(T t)`, there is no
    // corresponding constructor.
    $1 = std::move(cppval);
    // Define $result for the `out` typemap.
    $typemap(jni, CType) $result;
    // Call the `out` typemap to set the value of $result.
    $typemap(out, CType);
    return $result;
  }(std::move($1).value());
  if (jenv->ExceptionCheck()) {
    return $null;
  }
  $result = std::move(jval);
}
%enddef  // STATUSOR_RETURN
