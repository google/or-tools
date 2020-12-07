// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_STL_LOGGING_H_
#define OR_TOOLS_BASE_STL_LOGGING_H_

#include <deque>
#include <list>
#include <map>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"

// Forward declare these two, and define them after all the container streams
// operators so that we can recurse from pair -> container -> container -> pair
// properly.
template <class First, class Second>
std::ostream& operator<<(std::ostream& out, const std::pair<First, Second>& p);

namespace google {

template <class Iter>
void PrintSequence(std::ostream& out, Iter begin, Iter end);

}  // namespace google

#define OUTPUT_TWO_ARG_CONTAINER(Sequence)                       \
  template <class T1, class T2>                                  \
  inline std::ostream& operator<<(std::ostream& out,             \
                                  const Sequence<T1, T2>& seq) { \
    google::PrintSequence(out, seq.begin(), seq.end());          \
    return out;                                                  \
  }

OUTPUT_TWO_ARG_CONTAINER(std::vector)
OUTPUT_TWO_ARG_CONTAINER(std::deque)
OUTPUT_TWO_ARG_CONTAINER(std::list)

#undef OUTPUT_TWO_ARG_CONTAINER

#define OUTPUT_THREE_ARG_CONTAINER(Sequence)                         \
  template <class T1, class T2, class T3>                            \
  inline std::ostream& operator<<(std::ostream& out,                 \
                                  const Sequence<T1, T2, T3>& seq) { \
    google::PrintSequence(out, seq.begin(), seq.end());              \
    return out;                                                      \
  }

OUTPUT_THREE_ARG_CONTAINER(std::set)
OUTPUT_THREE_ARG_CONTAINER(std::multiset)

#undef OUTPUT_THREE_ARG_CONTAINER

#define OUTPUT_FOUR_ARG_CONTAINER(Sequence)                              \
  template <class T1, class T2, class T3, class T4>                      \
  inline std::ostream& operator<<(std::ostream& out,                     \
                                  const Sequence<T1, T2, T3, T4>& seq) { \
    google::PrintSequence(out, seq.begin(), seq.end());                  \
    return out;                                                          \
  }

OUTPUT_FOUR_ARG_CONTAINER(std::map)
OUTPUT_FOUR_ARG_CONTAINER(std::multimap)
OUTPUT_FOUR_ARG_CONTAINER(absl::flat_hash_set)
OUTPUT_FOUR_ARG_CONTAINER(absl::node_hash_set)

#undef OUTPUT_FOUR_ARG_CONTAINER

#define OUTPUT_FIVE_ARG_CONTAINER(Sequence)                                  \
  template <class T1, class T2, class T3, class T4, class T5>                \
  inline std::ostream& operator<<(std::ostream& out,                         \
                                  const Sequence<T1, T2, T3, T4, T5>& seq) { \
    google::PrintSequence(out, seq.begin(), seq.end());                      \
    return out;                                                              \
  }

OUTPUT_FIVE_ARG_CONTAINER(absl::flat_hash_map)
OUTPUT_FIVE_ARG_CONTAINER(absl::node_hash_map)

#undef OUTPUT_FIVE_ARG_CONTAINER

template <class First, class Second>
inline std::ostream& operator<<(std::ostream& out,
                                const std::pair<First, Second>& p) {
  out << '(' << p.first << ", " << p.second << ')';
  return out;
}

namespace google {

template <class Iter>
inline void PrintSequence(std::ostream& out, Iter begin, Iter end) {
  // Output at most 100 elements -- appropriate if used for logging.
  for (int i = 0; begin != end && i < 100; ++i, ++begin) {
    if (i > 0) out << ' ';
    out << *begin;
  }
  if (begin != end) {
    out << " ...";
  }
}

}  // namespace google

// Note that this is technically undefined behavior! We are adding things into
// the std namespace for a reason though -- we are providing new operations on
// types which are themselves defined with this namespace. Without this, these
// operator overloads cannot be found via ADL. If these definitions are not
// found via ADL, they must be #included before they're used, which requires
// this header to be included before apparently independent other headers.
//
// For example, base/logging.h defines various template functions to implement
// CHECK_EQ(x, y) and stream x and y into the log in the event the check fails.
// It does so via the function template MakeCheckOpValueString:
//   template<class T>
//   void MakeCheckOpValueString(strstream* ss, const T& v) {
//     (*ss) << v;
//   }
// Because 'logging.h' is included before 'stl_logging.h',
// subsequent CHECK_EQ(v1, v2) for vector<...> typed variable v1 and v2 can only
// find these operator definitions via ADL.
//
// Even this solution has problems -- it may pull unintended operators into the
// namespace as well, allowing them to also be found via ADL, and creating code
// that only works with a particular order of includes. Long term, we need to
// move all of the *definitions* into namespace std, bet we need to ensure no
// one references them first. This lets us take that step. We cannot define them
// in both because that would create ambiguous overloads when both are found.
namespace std {
using ::operator<<;
}

#endif  // OR_TOOLS_BASE_STL_LOGGING_H_
