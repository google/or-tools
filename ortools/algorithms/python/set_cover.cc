// Copyright 2010-2024 Google LLC
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

// A pybind11 wrapper for set_cover_*.

#include <memory>

#include "absl/base/nullability.h"
#include "ortools/algorithms/set_cover_heuristics.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/algorithms/set_cover_reader.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::ElementDegreeSolutionGenerator;
using ::operations_research::GreedySolutionGenerator;
using ::operations_research::GuidedLocalSearch;
using ::operations_research::Preprocessor;
using ::operations_research::RandomSolutionGenerator;
using ::operations_research::ReadBeasleySetCoverProblem;
using ::operations_research::ReadRailSetCoverProblem;
using ::operations_research::SetCoverInvariant;
using ::operations_research::SetCoverModel;
using ::operations_research::SteepestSearch;
using ::operations_research::SubsetIndex;
using ::operations_research::TrivialSolutionGenerator;

namespace py = pybind11;
using ::py::arg;

// General note about TODOs: the corresponding functions/classes/methods are
// more complex to wrap, as they use nonstandard types, and are less important,
// as they are not as useful to most users (mostly useful to write some custom
// Python heuristics).

PYBIND11_MODULE(set_cover, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  // set_cover_model.h
  py::class_<SetCoverModel>(m, "SetCoverModel")
      .def(py::init<>())
      .def_property_readonly("num_elements", &SetCoverModel::num_elements)
      .def_property_readonly("num_subsets", &SetCoverModel::num_subsets)
      .def_property_readonly("num_nonzeros", &SetCoverModel::num_nonzeros)
      .def_property_readonly("fill_rate", &SetCoverModel::FillRate)
      .def("add_empty_subset", &SetCoverModel::AddEmptySubset, arg("cost"))
      .def(
          "add_element_to_last_subset",
          [](SetCoverModel& model, int element) {
            model.AddElementToLastSubset(element);
          },
          arg("element"))
      .def(
          "set_subset_cost",
          [](SetCoverModel& model, int subset, double cost) {
            model.SetSubsetCost(subset, cost);
          },
          arg("subset"), arg("cost"))
      .def(
          "add_element_to_subset",
          [](SetCoverModel& model, int element, int subset) {
            model.AddElementToSubset(element, subset);
          },
          arg("subset"), arg("cost"))
      .def("compute_feasibility", &SetCoverModel::ComputeFeasibility)
      .def(
          "reserve_num_subsets",
          [](SetCoverModel& model, int num_subsets) {
            model.ReserveNumSubsets(num_subsets);
          },
          arg("num_subsets"))
      .def(
          "reserve_num_elements_in_subset",
          [](SetCoverModel& model, int num_elements, int subset) {
            model.ReserveNumElementsInSubset(num_elements, subset);
          },
          arg("num_elements"), arg("subset"))
      .def("export_model_as_proto", &SetCoverModel::ExportModelAsProto)
      .def("import_model_from_proto", &SetCoverModel::ImportModelFromProto);
  // TODO(user): add support for subset_costs, columns, rows,
  // row_view_is_valid, SubsetRange, ElementRange, all_subsets,
  // CreateSparseRowView, ComputeCostStats, ComputeRowStats,
  // ComputeColumnStats, ComputeRowDeciles, ComputeColumnDeciles.

  // TODO(user): wrap IntersectingSubsetsIterator.

  // set_cover_invariant.h
  py::class_<SetCoverInvariant>(m, "SetCoverInvariant")
      .def(py::init<SetCoverModel*>())
      .def("initialize", &SetCoverInvariant::Initialize)
      .def("clear", &SetCoverInvariant::Clear)
      .def("recompute_invariant", &SetCoverInvariant::RecomputeInvariant)
      .def("model", &SetCoverInvariant::model)
      .def_property(
          "model",
          // Expected semantics: give a pointer to Python **while
          // keeping ownership** in C++.
          [](SetCoverInvariant& invariant) -> std::shared_ptr<SetCoverModel> {
            // https://pybind11.readthedocs.io/en/stable/advanced/smart_ptrs.html#std-shared-ptr
            std::shared_ptr<SetCoverModel> ptr(invariant.model());
            return ptr;
          },
          [](SetCoverInvariant& invariant, const SetCoverModel& model) {
            *invariant.model() = model;
          })
      .def("cost", &SetCoverInvariant::cost)
      .def("num_uncovered_elements", &SetCoverInvariant::num_uncovered_elements)
      .def("clear_trace", &SetCoverInvariant::ClearTrace)
      .def("clear_removability_information",
           &SetCoverInvariant::ClearRemovabilityInformation)
      .def("compress_trace", &SetCoverInvariant::CompressTrace)
      .def("check_consistency", &SetCoverInvariant::CheckConsistency)
      .def(
          "flip",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.Flip(SubsetIndex(subset));
          },
          arg("subset"))
      .def(
          "flip_and_fully_update",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.FlipAndFullyUpdate(SubsetIndex(subset));
          },
          arg("subset"))
      .def(
          "select",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.Select(SubsetIndex(subset));
          },
          arg("subset"))
      .def(
          "select_and_fully_update",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.SelectAndFullyUpdate(SubsetIndex(subset));
          },
          arg("subset"))
      .def(
          "deselect",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.Deselect(SubsetIndex(subset));
          },
          arg("subset"))
      .def(
          "deselect_and_fully_update",
          [](SetCoverInvariant& invariant, int subset) {
            invariant.DeselectAndFullyUpdate(SubsetIndex(subset));
          },
          arg("subset"))
      .def("export_solution_as_proto",
           &SetCoverInvariant::ExportSolutionAsProto)
      .def("import_solution_from_proto",
           &SetCoverInvariant::ImportSolutionFromProto);
  // TODO(user): add support for is_selected, num_free_elements,
  // num_coverage_le_1_elements, coverage, ComputeCoverageInFocus,
  // is_redundant, trace, new_removable_subsets, new_non_removable_subsets,
  // LoadSolution, ComputeIsRedundant.

  // set_cover_heuristics.h
  py::class_<Preprocessor>(m, "Preprocessor")
      .def(py::init<absl::Nonnull<SetCoverInvariant*>>())
      .def("next_solution",
           [](Preprocessor& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("num_columns_fixed_by_singleton_row",
           &Preprocessor::num_columns_fixed_by_singleton_row);
  // TODO(user): add support for focus argument.

  py::class_<TrivialSolutionGenerator>(m, "TrivialSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution", [](TrivialSolutionGenerator& heuristic) -> bool {
        return heuristic.NextSolution();
      });
  // TODO(user): add support for focus argument.

  py::class_<RandomSolutionGenerator>(m, "RandomSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution", [](RandomSolutionGenerator& heuristic) -> bool {
        return heuristic.NextSolution();
      });
  // TODO(user): add support for focus argument.

  py::class_<GreedySolutionGenerator>(m, "GreedySolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution", [](GreedySolutionGenerator& heuristic) -> bool {
        return heuristic.NextSolution();
      });
  // TODO(user): add support for focus and cost arguments.

  py::class_<ElementDegreeSolutionGenerator>(m,
                                             "ElementDegreeSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](ElementDegreeSolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           });
  // TODO(user): add support for focus and cost arguments.

  py::class_<SteepestSearch>(m, "SteepestSearch")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](SteepestSearch& heuristic, int num_iterations) -> bool {
             return heuristic.NextSolution(num_iterations);
           });
  // TODO(user): add support for focus and cost arguments.

  py::class_<GuidedLocalSearch>(m, "GuidedLocalSearch")
      .def(py::init<SetCoverInvariant*>())
      .def("initialize", &GuidedLocalSearch::Initialize)
      .def("next_solution",
           [](GuidedLocalSearch& heuristic, int num_iterations) -> bool {
             return heuristic.NextSolution(num_iterations);
           });
  // TODO(user): add support for focus and cost arguments.

  // TODO(user): add support for ClearRandomSubsets, ClearRandomSubsets,
  // ClearMostCoveredElements, ClearMostCoveredElements, TabuList,
  // GuidedTabuSearch.

  // set_cover_reader.h
  m.def("read_beasly_set_cover_problem", &ReadBeasleySetCoverProblem);
  m.def("read_rail_set_cover_problem", &ReadRailSetCoverProblem);

  // set_cover_lagrangian.h
  // TODO(user): add support for SetCoverLagrangian.
}
