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
 * absl_string_view.swig
 *
 * Typemaps for absl::string_view
 * This is mapped to a Java String and is passed around by value.
 * -------------------------------------------------------------------------- */

%{
#include "absl/strings/string_view.h"
#include <string>
%}

namespace absl {

%naturalvar string_view;

class string_view;

// string_view
%typemap(jni) string_view "jstring"
%typemap(jtype) string_view "String"
%typemap(jstype) string_view "String"
%typemap(javadirectorin) string_view "$jniinput"
%typemap(javadirectorout) string_view "$javacall"

%typemap(in) string_view
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
   }
   const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
   if (!$1_pstr) return $null;
   $1 = absl::string_view($1_pstr); %}

/* absl::string_view requires the string data to remain valid while the
 * string_view is in use. */
%typemap(freearg) string_view
%{ jenv->ReleaseStringUTFChars($input, $1_pstr); %}

%typemap(directorout,warning=SWIGWARN_TYPEMAP_THREAD_UNSAFE_MSG) string_view
%{ if(!$input) {
     if (!jenv->ExceptionCheck()) {
       SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     }
     return $null;
   }
   const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
   if (!$1_pstr) return $null;
   /* possible thread/reentrant code problem */
   static std::string $1_str;
   $1_str = $1_pstr;
   $result = absl::string_view($1_str);
   jenv->ReleaseStringUTFChars($input, $1_pstr); %}

/* absl::string_view::data() isn't zero-byte terminated, but NewStringUTF()
 * requires a zero byte so it seems we have to make a copy (ick).  The
 * cleanest way to do that seems to be via a temporary std::string.
 */
%typemap(directorin,descriptor="Ljava/lang/String;") string_view
%{ $input = jenv->NewStringUTF(std::string($1).c_str());
   Swig::LocalRefGuard $1_refguard(jenv, $input); %}

%typemap(out) string_view
%{ $result = jenv->NewStringUTF(std::string($1).c_str()); %}

%typemap(javain) string_view "$javainput"

%typemap(javaout) string_view {
    return $jnicall;
  }

%typemap(typecheck) string_view = char *;

%typemap(throws) string_view
%{ SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException, std::string($1).c_str());
   return $null; %}

}  // namespace absl
