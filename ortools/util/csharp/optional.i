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


%include "ortools/base/base.i"

%{
#include <cstdint>
#include <optional>
%}

namespace std {
  template<typename T> class optional {
    public:
      typedef T value_type;

      optional();
      optional(T value);
      optional(const optional& other);

      constexpr bool has_value() const noexcept;
      constexpr const T& value() const&;
  };
}

// ============================================================================
// STL_OPTIONAL(TYPE) - For primitive types with nullable value types
// ============================================================================
%define STL_OPTIONAL(TYPE)

%naturalvar std::optional<TYPE>;

// C# type is the existing cstype for TYPE with ? appended
// isnullablevaluetype attribute tells SWIG to keep the ? in typeof() for director method types
%typemap(cstype, isnullablevaluetype="1") std::optional<TYPE> "$typemap(cstype, TYPE)?"
%typemap(imtype, out="global::System.IntPtr") std::optional<TYPE> "global::System.IntPtr"
%typemap(ctype) std::optional<TYPE> "void *"
// C++ output typemaps - return pointer to static storage
%typemap(out) std::optional<TYPE> %{
  static TYPE $1_static_val;
  if ($1.has_value()) {
    $1_static_val = $1.value();
    $result = &$1_static_val;
  } else {
    $result = nullptr;
  }
%}
// Convert IntPtr to C# nullable for output
%typemap(csout, excode=SWIGEXCODE) std::optional<TYPE> {
  global::System.IntPtr cPtr = $imcall;$excode;
  if (cPtr != global::System.IntPtr.Zero) {
    return
      global::System.Runtime.InteropServices.Marshal.PtrToStructure<$typemap(cstype,TYPE)>(cPtr);
  }
  return ($typemap(cstype, TYPE)?)null;
}

// Instantiate the template with empty name to prevent SWIGTYPE generation
%template() std::optional<TYPE>;

%enddef

