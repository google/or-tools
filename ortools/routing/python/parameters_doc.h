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

#ifndef ORTOOLS_ROUTING_PYTHON_PARAMETERS_DOC_H_
#define ORTOOLS_ROUTING_PYTHON_PARAMETERS_DOC_H_
// clang-format off
/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define MKD_EXPAND(x)                                      x
#define MKD_COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define MKD_VA_SIZE(...)                                   MKD_EXPAND(MKD_COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define MKD_CAT1(a, b)                                     a ## b
#define MKD_CAT2(a, b)                                     MKD_CAT1(a, b)
#define MKD_DOC1(n1)                                       mkd_doc_##n1
#define MKD_DOC2(n1, n2)                                   mkd_doc_##n1##_##n2
#define MKD_DOC3(n1, n2, n3)                               mkd_doc_##n1##_##n2##_##n3
#define MKD_DOC4(n1, n2, n3, n4)                           mkd_doc_##n1##_##n2##_##n3##_##n4
#define MKD_DOC5(n1, n2, n3, n4, n5)                       mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5
#define MKD_DOC6(n1, n2, n3, n4, n5, n6)                   mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define MKD_DOC7(n1, n2, n3, n4, n5, n6, n7)               mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                           MKD_EXPAND(MKD_EXPAND(MKD_CAT2(MKD_DOC, MKD_VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *mkd_doc_operations_research_routing_DefaultRoutingModelParameters = R"doc()doc";

static const char *mkd_doc_operations_research_routing_DefaultRoutingSearchParameters = R"doc()doc";

static const char *mkd_doc_operations_research_routing_DefaultSecondaryRoutingSearchParameters = R"doc()doc";

static const char *mkd_doc_operations_research_routing_FindErrorInRoutingSearchParameters =
R"doc(Returns an empty std::string if the routing search parameters are
valid, and a non-empty, human readable error description if they're
not.)doc";

static const char *mkd_doc_operations_research_routing_FindErrorsInIteratedLocalSearchParameters =
R"doc(Returns a list of std::string describing the errors in the iterated
local search parameters. Returns an empty vector if the parameters are
valid.)doc";

static const char *mkd_doc_operations_research_routing_FindErrorsInRoutingSearchParameters =
R"doc(Returns a list of std::string describing the errors in the routing
search parameters. Returns an empty vector if the parameters are
valid.)doc";

static const char *mkd_doc_operations_research_routing_RecommendedIteratedLocalSearchParameters = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

// clang-format on
#endif  // ORTOOLS_ROUTING_PYTHON_PARAMETERS_DOC_H_
