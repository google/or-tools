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

#include "ortools/graph/max_flow.h"

#include <vector>

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

using ::operations_research::SimpleMaxFlow;
using ::pybind11::arg;

PYBIND11_MODULE(max_flow, m) {
  pybind11::class_<SimpleMaxFlow> smf(m, "SimpleMaxFlow");
  smf.def(pybind11::init<>());
  smf.def("add_arc_with_capacity", &SimpleMaxFlow::AddArcWithCapacity,
          arg("tail"), arg("head"), arg("capacity"));
  smf.def("add_arcs_with_capacity",
          pybind11::vectorize(&SimpleMaxFlow::AddArcWithCapacity));
  smf.def("set_arc_capacity", &SimpleMaxFlow::SetArcCapacity, arg("arc"),
          arg("capacity"));
  smf.def("set_arcs_capacity",
          pybind11::vectorize(&SimpleMaxFlow::SetArcCapacity));
  smf.def("num_nodes", &SimpleMaxFlow::NumNodes);
  smf.def("num_arcs", &SimpleMaxFlow::NumArcs);
  smf.def("tail", &SimpleMaxFlow::Tail, arg("arc"));
  smf.def("head", &SimpleMaxFlow::Head, arg("arc"));
  smf.def("capacity", &SimpleMaxFlow::Capacity, arg("arc"));
  smf.def("solve", &SimpleMaxFlow::Solve, arg("source"), arg("sink"));
  smf.def("optimal_flow", &SimpleMaxFlow::OptimalFlow);
  smf.def("flow", &SimpleMaxFlow::Flow, arg("arc"));
  smf.def("flows", pybind11::vectorize(&SimpleMaxFlow::Flow));
  smf.def("get_source_side_min_cut", [](SimpleMaxFlow* smf) {
    std::vector<SimpleMaxFlow::NodeIndex> result;
    smf->GetSourceSideMinCut(&result);
    return result;
  });
  smf.def("get_sink_side_min_cut", [](SimpleMaxFlow* smf) {
    std::vector<SimpleMaxFlow::NodeIndex> result;
    smf->GetSinkSideMinCut(&result);
    return result;
  });

  pybind11::enum_<SimpleMaxFlow::Status>(smf, "Status")
      .value("OPTIMAL", SimpleMaxFlow::Status::OPTIMAL)
      .value("POSSIBLE_OVERFLOW", SimpleMaxFlow::Status::POSSIBLE_OVERFLOW)
      .value("BAD_INPUT", SimpleMaxFlow::Status::BAD_INPUT)
      .value("BAD_RESULT", SimpleMaxFlow::Status::BAD_RESULT)
      .export_values();
}
