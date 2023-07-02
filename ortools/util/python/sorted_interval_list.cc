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

#include "ortools/util/sorted_interval_list.h"

#include <cstdint>

#include "ortools/util/python/sorted_interval_list_doc.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

using ::operations_research::Domain;
using ::pybind11::arg;

PYBIND11_MODULE(sorted_interval_list, m) {
  pybind11::class_<Domain>(m, "Domain", DOC(operations_research, Domain))
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
      .def(pybind11::init<int64_t, int64_t>(),
           DOC(operations_research, Domain, Domain))
      .def("AdditionWith", &Domain::AdditionWith,
           DOC(operations_research, Domain, AdditionWith), arg("domain"))
      .def("Complement", &Domain::Complement,
           DOC(operations_research, Domain, Complement))
      .def("Contains", &Domain::Contains,
           DOC(operations_research, Domain, Contains), arg("value"))
      .def("FlattenedIntervals", &Domain::FlattenedIntervals,
           DOC(operations_research, Domain, FlattenedIntervals))
      .def("IntersectionWith", &Domain::IntersectionWith,
           DOC(operations_research, Domain, IntersectionWith), arg("domain"))
      .def("IsEmpty", &Domain::IsEmpty,
           DOC(operations_research, Domain, IsEmpty))
      .def("Size", &Domain::Size, DOC(operations_research, Domain, Size))
      .def("Max", &Domain::Max, DOC(operations_research, Domain, Max))
      .def("Min", &Domain::Min, DOC(operations_research, Domain, Min))
      .def("Negation", &Domain::Negation,
           DOC(operations_research, Domain, Negation))
      .def("UnionWith", &Domain::UnionWith,
           DOC(operations_research, Domain, UnionWith), arg("domain"))
      .def("__str__", &Domain::ToString);
}
