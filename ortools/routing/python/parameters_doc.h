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

/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x) x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define __VA_SIZE(...) __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b) a##b
#define __CAT2(a, b) __CAT1(a, b)
#define __DOC1(n1) __doc_##n1
#define __DOC2(n1, n2) __doc_##n1##_##n2
#define __DOC3(n1, n2, n3) __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4) __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5) __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...) \
  __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static const char* __doc_operations_research_DefaultRoutingModelParameters =
    R"doc()doc";

static const char* __doc_operations_research_DefaultRoutingSearchParameters =
    R"doc()doc";

static const char*
    __doc_operations_research_DefaultSecondaryRoutingSearchParameters =
        R"doc()doc";

static const char*
    __doc_operations_research_FindErrorInRoutingSearchParameters =
        R"doc(Returns an empty std::string if the routing search parameters are
valid, and a non-empty, human readable error description if they're
not.)doc";

static const char*
    __doc_operations_research_FindErrorsInRoutingSearchParameters =
        R"doc(Returns a list of std::string describing the errors in the routing
search parameters. Returns an empty vector if the parameters are
valid.)doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
