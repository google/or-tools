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

#include "ortools/util/sorted_interval_list.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "ortools/util/python/sorted_interval_list_doc.h"
#include "pybind11/cast.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

using ::operations_research::Domain;
using ::pybind11::arg;

PYBIND11_MODULE(sorted_interval_list, m) {
  pybind11::class_<Domain>(m, "Domain", DOC(operations_research, Domain))
      .def_static("all_values", &Domain::AllValues,
                  DOC(operations_research, Domain, AllValues))
      .def_static("greater_or_equal", &Domain::GreaterOrEqual, arg("value"),
                  DOC(operations_research, Domain, GreaterOrEqual))
      .def_static("from_values", &Domain::FromValues,
                  DOC(operations_research, Domain, FromValues), arg("values"))
      .def_static("from_intervals", &Domain::FromVectorIntervals,
                  DOC(operations_research, Domain, FromVectorIntervals),
                  arg("intervals"))
      .def_static("from_flat_intervals", &Domain::FromFlatIntervals,
                  DOC(operations_research, Domain, FromFlatIntervals),
                  arg("flat_intervals"))
      .def_static("lower_or_equal", &Domain::LowerOrEqual, arg("value"),
                  DOC(operations_research, Domain, LowerOrEqual))
      .def(pybind11::init<int64_t, int64_t>(),
           DOC(operations_research, Domain, Domain))
      .def("addition_with", &Domain::AdditionWith,
           DOC(operations_research, Domain, AdditionWith), arg("domain"))
      .def("complement", &Domain::Complement,
           DOC(operations_research, Domain, Complement))
      .def("contains", &Domain::Contains,
           DOC(operations_research, Domain, Contains), arg("value"))
      .def("flattened_intervals", &Domain::FlattenedIntervals,
           DOC(operations_research, Domain, FlattenedIntervals))
      .def("intersection_with", &Domain::IntersectionWith,
           DOC(operations_research, Domain, IntersectionWith), arg("domain"))
      .def("is_empty", &Domain::IsEmpty,
           DOC(operations_research, Domain, IsEmpty))
      .def("is_included_in", &Domain::IsIncludedIn,
           DOC(operations_research, Domain, IsIncludedIn), arg("domain"))
      .def("size", &Domain::Size, DOC(operations_research, Domain, Size))
      .def("max", &Domain::Max, DOC(operations_research, Domain, Max))
      .def("min", &Domain::Min, DOC(operations_research, Domain, Min))
      .def("negation", &Domain::Negation,
           DOC(operations_research, Domain, Negation))
      .def("overlaps_with", &Domain::OverlapsWith,
           DOC(operations_research, Domain, OverlapsWith), arg("domain"))
      .def("union_with", &Domain::UnionWith,
           DOC(operations_research, Domain, UnionWith), arg("domain"))
      .def("__str__", &Domain::ToString)
      .def("__repr__",
           [](const Domain& domain) {
             return absl::StrCat("Domain(", domain.ToString(), ")");
           })
      // Compatibility with pre PEP8 APIs.
      .def_static("AllValues", &Domain::AllValues,
                  DOC(operations_research, Domain, AllValues))
      .def_static("FromValues", &Domain::FromValues,
                  DOC(operations_research, Domain, FromValues), arg("values"))
      .def_static("FromIntervals", &Domain::FromVectorIntervals,
                  DOC(operations_research, Domain, FromVectorIntervals),
                  arg("intervals"))
      .def_static("FromFlatIntervals", &Domain::FromFlatIntervals,
                  DOC(operations_research, Domain, FromFlatIntervals),
                  arg("flat_intervals"))
      .def("FlattenedIntervals", &Domain::FlattenedIntervals,
           DOC(operations_research, Domain, FlattenedIntervals));
}
