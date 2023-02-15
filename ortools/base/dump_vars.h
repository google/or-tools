// Copyright 2010-2022 Google LLC
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

// DUMP_VARS() is a convenience macro for writing objects to text logs. It
// prints all of its arguments as key-value pairs.
//
// Example:
//   int foo = 42;
//   vector<int> bar = {1, 2, 3};
//   // Prints: foo = 42, bar.size() = 3
//   LOG(INFO) << DUMP_VARS(foo, bar.size());
//
// DUMP_VARS() produces high quality human-readable output for most types:
// builtin types, strings, anything with operator<<.
//
//                    ====[ Limitations ]====
//
// DUMP_VARS() accepts at most 8 arguments.
//
// Structured bindings require an extra step to make DUMP_VARS print them. They
// need to be listed as first argument of DUMP_VARS_WITH_BINDINGS:
//
//   for (const auto& [x, y, z] : Foo()) {
//     // Would not compile:
//     // LOG(INFO) << DUMP_VARS(x, *y, f(z), other_var);
//     LOG(INFO) << DUMP_VARS_WITH_BINDINGS((x, y, z), x, *y, f(z), other_var);
//   }

#ifndef OR_TOOLS_BASE_DUMP_VARS_H_
#define OR_TOOLS_BASE_DUMP_VARS_H_

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"

/* need extra level to force extra eval */
#define DUMP_FOR_EACH_N0(F)
#define DUMP_FOR_EACH_N1(F, a) F(a)
#define DUMP_FOR_EACH_N2(F, a, ...) F(a) DUMP_FOR_EACH_N1(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N3(F, a, ...) F(a) DUMP_FOR_EACH_N2(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N4(F, a, ...) F(a) DUMP_FOR_EACH_N3(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N5(F, a, ...) F(a) DUMP_FOR_EACH_N4(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N6(F, a, ...) F(a) DUMP_FOR_EACH_N5(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N7(F, a, ...) F(a) DUMP_FOR_EACH_N6(F, __VA_ARGS__)
#define DUMP_FOR_EACH_N8(F, a, ...) F(a) DUMP_FOR_EACH_N7(F, __VA_ARGS__)

#define DUMP_CONCATENATE(x, y) x##y
#define DUMP_FOR_EACH_(N, F, ...) \
  DUMP_CONCATENATE(DUMP_FOR_EACH_N, N)(F __VA_OPT__(, __VA_ARGS__))

#define DUMP_NARG(...) DUMP_NARG_(__VA_OPT__(__VA_ARGS__, ) DUMP_RSEQ_N())
#define DUMP_NARG_(...) DUMP_ARG_N(__VA_ARGS__)
#define DUMP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define DUMP_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0
#define DUMP_FOR_EACH(F, ...) \
  DUMP_FOR_EACH_(DUMP_NARG(__VA_ARGS__), F __VA_OPT__(, __VA_ARGS__))

#define DUMP_VARS(...) DUMP_VARS_WITH_BINDINGS((), __VA_ARGS__)

/* need extra level to force extra eval */
#define DUMP_STRINGIZE(a) #a,
#define DUMP_STRINGIFY(...) DUMP_FOR_EACH(DUMP_STRINGIZE, __VA_ARGS__)

// Returns the arguments.
#define DUMP_IDENTITY(...) __VA_ARGS__
// Removes parenthesis. Requires argument enclosed in parenthesis.
#define DUMP_RM_PARENS(...) DUMP_IDENTITY __VA_ARGS__

#define DUMP_GEN_ONE_BINDING(a) , &a = a
#define DUMP_GEN_BINDING(binding) \
  &DUMP_FOR_EACH(DUMP_GEN_ONE_BINDING, DUMP_RM_PARENS(binding))

#define DUMP_VARS_WITH_BINDINGS(binding, ...)                               \
  ::operations_research::base::internal_dump_vars::make_dump_vars<>(        \
      ::operations_research::base::internal_dump_vars::DumpNames{           \
          DUMP_STRINGIFY(__VA_ARGS__)},                                     \
      [DUMP_GEN_BINDING(binding)](                                          \
          ::std::ostream& os, const ::std::string& field_sep,               \
          const ::std::string& kv_sep,                                      \
          const ::operations_research::base::internal_dump_vars::DumpNames& \
              names) {                                                      \
        ::operations_research::base::internal_dump_vars::print_fields{      \
            .os = os,                                                       \
            .field_sep = field_sep,                                         \
            .kv_sep = kv_sep,                                               \
            .names = names,                                                 \
        }(__VA_ARGS__);                                                     \
      })

namespace operations_research::base {
namespace internal_dump_vars {

// needed by routing
template <typename T>
std::ostream& operator<<(std::ostream& os,
                         const ::absl::InlinedVector<T, 8>& vec) {
  for (T it : vec) {
    os << ::std::to_string(it) << ',';
  }
  return os;
}

using DumpNames = ::std::vector<::std::string>;

struct print_fields {
  void operator()() {}

  template <class T>
  void operator()(const T& t) {
    os << names[n++] << kv_sep << t;
  }

  template <class T1, class T2, class... Ts>
  void operator()(const T1& t1, const T2& t2, const Ts&... ts) {
    os << names[n++] << kv_sep << t1 << field_sep;
    (*this)(t2, ts...);
  }

  std::ostream& os;
  const ::std::string& field_sep;
  const ::std::string& kv_sep;
  const DumpNames& names;
  ::std::size_t n = 0;
};

template <class F>
class Dump {
 public:
  explicit Dump(const ::std::string&& field_sep, const ::std::string&& kv_sep,
                DumpNames&& names, F f)
      : field_sep_(::std::move(field_sep)),
        kv_sep_(::std::move(kv_sep)),
        names_(::std::move(names)),
        f_(::std::move(f)) {}

  ::std::string str() const {
    ::std::ostringstream oss;
    oss << *this;
    return oss.str();
  }

  template <class... N>
  Dump<F> as(N&&... names) const {
    return Dump<F>(::std::string{field_sep_}, ::std::string{kv_sep_},
                   DumpNames{names...}, f_);
  }

  Dump& sep(::std::string&& field_sep) {
    field_sep_ = ::std::move(field_sep);
    return *this;
  }

  Dump& sep(::std::string&& field_sep, ::std::string&& kv_sep) {
    field_sep_ = ::std::move(field_sep);
    kv_sep_ = ::std::move(kv_sep);
    return *this;
  }

  friend ::std::ostream& operator<<(::std::ostream& os, const Dump& dump) {
    dump.print_fields_(os);
    return os;
  }

 private:
  void print_fields_(::std::ostream& os) const {
    f_(os, field_sep_, kv_sep_, names_);
  }

  ::std::string field_sep_;
  ::std::string kv_sep_;
  DumpNames names_;
  F f_;
};

template <class F>
Dump<F> make_dump_vars(DumpNames&& names, F f) {
  return Dump<F>(
      /*field_sep=*/", ",
      /*kv_sep=*/" = ", ::std::move(names), ::std::move(f));
}

}  // namespace internal_dump_vars
}  // namespace operations_research::base

#endif  // OR_TOOLS_BASE_DUMP_VARS_H_
