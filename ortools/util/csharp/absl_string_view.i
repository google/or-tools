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

/* -----------------------------------------------------------------------------
 * std_string_view.i
 *
 * Typemaps for absl::string_view
 * This is mapped to a C# String and is passed around by value.
 * ----------------------------------------------------------------------------- */

%{
#include "absl/strings/string_view.h"
#include <string>
%}

namespace absl {

%naturalvar string_view;

class string_view;

// string_view
%typemap(ctype) string_view "const char *"
%typemap(imtype) string_view "string"
%typemap(cstype) string_view "string"

%typemap(csdirectorin) string_view "$iminput"
%typemap(csdirectorout) string_view "$cscall"

%typemap(in, canthrow=1) string_view
%{ if (!$input) {
    SWIG_CSharpSetPendingExceptionArgument(SWIG_CSharpArgumentNullException, "null string", 0);
    return $null;
   }
   $1 = absl::string_view($input); %}
%typemap(out) string_view %{ $result = SWIG_csharp_string_callback(std::string($1).c_str()); %}

%typemap(directorout, canthrow=1, warning=SWIGWARN_TYPEMAP_THREAD_UNSAFE_MSG) string_view
%{ if (!$input) {
    SWIG_CSharpSetPendingExceptionArgument(SWIG_CSharpArgumentNullException, "null string", 0);
    return $null;
   }
   /* possible thread/reentrant code problem */
   static std::string $1_str;
   $1_str = $input;
   $result = absl::string_view($1_str); %}

%typemap(directorin) string_view %{ $input = std::string($1).c_str(); %}

%typemap(csin) string_view "$csinput"
%typemap(csout, excode=SWIGEXCODE) string_view {
    string ret = $imcall;$excode
    return ret;
  }

%typemap(typecheck) string_view = char *;

%typemap(throws, canthrow=1) string_view
%{ SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, std::string($1).c_str());
   return $null; %}

}  // namespace absl
