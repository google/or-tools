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

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "ortools/algorithms/set_cover_heuristics.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/algorithms/set_cover_reader.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::BaseInt;
using ::operations_research::ClearRandomSubsets;
using ::operations_research::ElementDegreeSolutionGenerator;
using ::operations_research::ElementIndex;
using ::operations_research::GreedySolutionGenerator;
using ::operations_research::GuidedLocalSearch;
using ::operations_research::GuidedTabuSearch;
using ::operations_research::LazyElementDegreeSolutionGenerator;
using ::operations_research::RandomSolutionGenerator;
using ::operations_research::ReadFimiDat;
using ::operations_research::ReadOrlibRail;
using ::operations_research::ReadOrlibScp;
using ::operations_research::ReadSetCoverProto;
using ::operations_research::ReadSetCoverSolutionProto;
using ::operations_research::ReadSetCoverSolutionText;
using ::operations_research::WriteOrlibRail;
using ::operations_research::WriteOrlibScp;
using ::operations_research::WriteSetCoverProto;
using ::operations_research::WriteSetCoverSolutionProto;
using ::operations_research::WriteSetCoverSolutionText;

using ::operations_research::SetCoverDecision;
using ::operations_research::SetCoverInvariant;
using ::operations_research::SetCoverModel;
using ::operations_research::SparseColumn;
using ::operations_research::SparseRow;
using ::operations_research::SteepestSearch;
using ::operations_research::SubsetBoolVector;
using ::operations_research::SubsetCostVector;
using ::operations_research::SubsetIndex;
using ::operations_research::TabuList;
using ::operations_research::TrivialSolutionGenerator;

namespace py = pybind11;
using ::py::arg;
using ::py::make_iterator;

std::vector<SubsetIndex> VectorIntToVectorSubsetIndex(
    absl::Span<const BaseInt> ints) {
  std::vector<SubsetIndex> subs;
  std::transform(ints.begin(), ints.end(), subs.begin(),
                 [](int subset) -> SubsetIndex { return SubsetIndex(subset); });
  return subs;
}

SubsetCostVector VectorDoubleToSubsetCostVector(
    absl::Span<const double> doubles) {
  SubsetCostVector costs(doubles.begin(), doubles.end());
  return costs;
}

class IntIterator {
 public:
  using value_type = int;
  using difference_type = std::ptrdiff_t;
  using pointer = int*;
  using reference = int&;
  using iterator_category = std::input_iterator_tag;

  explicit IntIterator(int max_value)
      : max_value_(max_value), current_value_(0) {}

  int operator*() const { return current_value_; }
  IntIterator& operator++() {
    ++current_value_;
    return *this;
  }

  static IntIterator begin(int max_value) { return IntIterator{max_value}; }
  static IntIterator end(int max_value) { return {max_value, max_value}; }

  friend bool operator==(const IntIterator& lhs, const IntIterator& rhs) {
    return lhs.max_value_ == rhs.max_value_ &&
           lhs.current_value_ == rhs.current_value_;
  }

 private:
  IntIterator(int max_value, int current_value)
      : max_value_(max_value), current_value_(current_value) {}

  const int max_value_;
  int current_value_;
};

PYBIND11_MODULE(set_cover, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  // set_cover_model.h
  py::class_<SetCoverModel::Stats>(m, "SetCoverModelStats")
      .def_readwrite("min", &SetCoverModel::Stats::min)
      .def_readwrite("max", &SetCoverModel::Stats::max)
      .def_readwrite("median", &SetCoverModel::Stats::median)
      .def_readwrite("mean", &SetCoverModel::Stats::mean)
      .def_readwrite("stddev", &SetCoverModel::Stats::stddev)
      .def("debug_string", &SetCoverModel::Stats::DebugString);

  py::class_<SetCoverModel>(m, "SetCoverModel")
      .def(py::init<>())
      .def_property_readonly("num_elements", &SetCoverModel::num_elements)
      .def_property_readonly("num_subsets", &SetCoverModel::num_subsets)
      .def_property_readonly("num_nonzeros", &SetCoverModel::num_nonzeros)
      .def_property_readonly("fill_rate", &SetCoverModel::FillRate)
      .def_property_readonly(
          "subset_costs",
          [](SetCoverModel& model) -> const std::vector<double>& {
            return model.subset_costs().get();
          })
      .def_property_readonly(
          "columns",
          [](SetCoverModel& model) -> std::vector<std::vector<BaseInt>> {
            // Due to the inner StrongVector, make a deep copy. Anyway,
            // columns() returns a const ref, so this keeps the semantics, not
            // the efficiency.
            std::vector<std::vector<BaseInt>> columns(model.columns().size());
            std::transform(
                model.columns().begin(), model.columns().end(), columns.begin(),
                [](const SparseColumn& column) -> std::vector<BaseInt> {
                  std::vector<BaseInt> col(column.size());
                  std::transform(column.begin(), column.end(), col.begin(),
                                 [](ElementIndex element) -> BaseInt {
                                   return element.value();
                                 });
                  return col;
                });
            return columns;
          })
      .def_property_readonly(
          "rows",
          [](SetCoverModel& model) -> std::vector<std::vector<BaseInt>> {
            // Due to the inner StrongVector, make a deep copy. Anyway,
            // rows() returns a const ref, so this keeps the semantics, not
            // the efficiency.
            std::vector<std::vector<BaseInt>> rows(model.rows().size());
            std::transform(model.rows().begin(), model.rows().end(),
                           rows.begin(),
                           [](const SparseRow& row) -> std::vector<BaseInt> {
                             std::vector<BaseInt> r(row.size());
                             std::transform(row.begin(), row.end(), r.begin(),
                                            [](SubsetIndex element) -> BaseInt {
                                              return element.value();
                                            });
                             return r;
                           });
            return rows;
          })
      .def_property_readonly("row_view_is_valid",
                             &SetCoverModel::row_view_is_valid)
      .def("SubsetRange",
           [](SetCoverModel& model) {
             return make_iterator<>(IntIterator::begin(model.num_subsets()),
                                    IntIterator::end(model.num_subsets()));
           })
      .def("ElementRange",
           [](SetCoverModel& model) {
             return make_iterator<>(IntIterator::begin(model.num_elements()),
                                    IntIterator::end(model.num_elements()));
           })
      .def_property_readonly("all_subsets",
                             [](SetCoverModel& model) -> std::vector<BaseInt> {
                               std::vector<BaseInt> subsets;
                               std::transform(
                                   model.all_subsets().begin(),
                                   model.all_subsets().end(), subsets.begin(),
                                   [](const SubsetIndex element) -> BaseInt {
                                     return element.value();
                                   });
                               return subsets;
                             })
      .def("add_empty_subset", &SetCoverModel::AddEmptySubset, arg("cost"))
      .def(
          "add_element_to_last_subset",
          [](SetCoverModel& model, BaseInt element) {
            model.AddElementToLastSubset(element);
          },
          arg("element"))
      .def(
          "set_subset_cost",
          [](SetCoverModel& model, BaseInt subset, double cost) {
            model.SetSubsetCost(subset, cost);
          },
          arg("subset"), arg("cost"))
      .def(
          "add_element_to_subset",
          [](SetCoverModel& model, BaseInt element, BaseInt subset) {
            model.AddElementToSubset(element, subset);
          },
          arg("subset"), arg("cost"))
      .def("create_sparse_row_view", &SetCoverModel::CreateSparseRowView)
      .def("sort_elements_in_subsets", &SetCoverModel::SortElementsInSubsets)
      .def("compute_feasibility", &SetCoverModel::ComputeFeasibility)
      .def(
          "reserve_num_subsets",
          [](SetCoverModel& model, BaseInt num_subsets) {
            model.ReserveNumSubsets(num_subsets);
          },
          arg("num_subsets"))
      .def(
          "reserve_num_elements_in_subset",
          [](SetCoverModel& model, BaseInt num_elements, BaseInt subset) {
            model.ReserveNumElementsInSubset(num_elements, subset);
          },
          arg("num_elements"), arg("subset"))
      .def("export_model_as_proto", &SetCoverModel::ExportModelAsProto)
      .def("import_model_from_proto", &SetCoverModel::ImportModelFromProto)
      .def("compute_cost_stats", &SetCoverModel::ComputeCostStats)
      .def("compute_row_stats", &SetCoverModel::ComputeRowStats)
      .def("compute_column_stats", &SetCoverModel::ComputeColumnStats)
      .def("compute_row_deciles", &SetCoverModel::ComputeRowDeciles)
      .def("compute_column_deciles", &SetCoverModel::ComputeRowDeciles);

  // TODO(user): wrap IntersectingSubsetsIterator.

  // set_cover_invariant.h
  py::class_<SetCoverDecision>(m, "SetCoverDecision")
      .def(py::init<>())
      .def(py::init([](BaseInt subset, bool value) -> SetCoverDecision* {
             return new SetCoverDecision(SubsetIndex(subset), value);
           }),
           arg("subset"), arg("value"))
      .def("subset",
           [](const SetCoverDecision& decision) -> BaseInt {
             return decision.subset().value();
           })
      .def("decision", &SetCoverDecision::decision);

  py::enum_<SetCoverInvariant::ConsistencyLevel>(m, "consistency_level")
      .value("COST_AND_COVERAGE",
             SetCoverInvariant::ConsistencyLevel::kCostAndCoverage)
      .value("FREE_AND_UNCOVERED",
             SetCoverInvariant::ConsistencyLevel::kFreeAndUncovered)
      .value("REDUNDANCY", SetCoverInvariant::ConsistencyLevel::kRedundancy);

  py::class_<SetCoverInvariant>(m, "SetCoverInvariant")
      .def(py::init<SetCoverModel*>())
      .def("initialize", &SetCoverInvariant::Initialize)
      .def("clear", &SetCoverInvariant::Clear)
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
      .def("is_selected",
           [](SetCoverInvariant& invariant) -> std::vector<bool> {
             return invariant.is_selected().get();
           })
      .def("num_free_elements",
           [](SetCoverInvariant& invariant) -> std::vector<BaseInt> {
             return invariant.num_free_elements().get();
           })
      .def("num_coverage_le_1_elements",
           [](SetCoverInvariant& invariant) -> std::vector<BaseInt> {
             return invariant.num_coverage_le_1_elements().get();
           })
      .def("coverage",
           [](SetCoverInvariant& invariant) -> std::vector<BaseInt> {
             return invariant.coverage().get();
           })
      .def(
          "compute_coverage_in_focus",
          [](SetCoverInvariant& invariant,
             const std::vector<BaseInt>& focus) -> std::vector<BaseInt> {
            return invariant
                .ComputeCoverageInFocus(VectorIntToVectorSubsetIndex(focus))
                .get();
          },
          arg("focus"))
      .def("is_redundant",
           [](SetCoverInvariant& invariant) -> std::vector<bool> {
             return invariant.is_redundant().get();
           })
      .def("trace", &SetCoverInvariant::trace)
      .def("clear_trace", &SetCoverInvariant::ClearTrace)
      .def("clear_removability_information",
           &SetCoverInvariant::ClearRemovabilityInformation)
      .def("newly_removable_subsets",
           &SetCoverInvariant::newly_removable_subsets)
      .def("newly_non_removable_subsets",
           &SetCoverInvariant::newly_non_removable_subsets)
      .def("compress_trace", &SetCoverInvariant::CompressTrace)
      .def("load_solution",
           [](SetCoverInvariant& invariant,
              const std::vector<bool>& solution) -> void {
             SubsetBoolVector sol(solution.begin(), solution.end());
             return invariant.LoadSolution(sol);
           })
      .def("check_consistency", &SetCoverInvariant::CheckConsistency)
      .def(
          "compute_is_redundant",
          [](SetCoverInvariant& invariant, BaseInt subset) -> bool {
            return invariant.ComputeIsRedundant(SubsetIndex(subset));
          },
          arg("subset"))
      .def("recompute", &SetCoverInvariant::Recompute)
      .def(
          "flip",
          [](SetCoverInvariant& invariant, BaseInt subset,
             SetCoverInvariant::ConsistencyLevel consistency) {
            invariant.Flip(SubsetIndex(subset), consistency);
          },
          arg("subset"), arg("consistency"))
      .def(
          "select",
          [](SetCoverInvariant& invariant, BaseInt subset,
             SetCoverInvariant::ConsistencyLevel consistency) {
            invariant.Select(SubsetIndex(subset), consistency);
          },
          arg("subset"), arg("consistency"))
      .def(
          "deselect",
          [](SetCoverInvariant& invariant, BaseInt subset,
             SetCoverInvariant::ConsistencyLevel consistency) {
            invariant.Deselect(SubsetIndex(subset), consistency);
          },
          arg("subset"), arg("consistency"))
      .def("export_solution_as_proto",
           &SetCoverInvariant::ExportSolutionAsProto)
      .def("import_solution_from_proto",
           &SetCoverInvariant::ImportSolutionFromProto);

  // set_cover_heuristics.h
  py::class_<TrivialSolutionGenerator>(m, "TrivialSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](TrivialSolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("next_solution",
           [](TrivialSolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus));
           });

  py::class_<RandomSolutionGenerator>(m, "RandomSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](RandomSolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("next_solution",
           [](RandomSolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus));
           });

  py::class_<GreedySolutionGenerator>(m, "GreedySolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](GreedySolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("next_solution",
           [](GreedySolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus));
           })
      .def("next_solution",
           [](GreedySolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus,
              absl::Span<const double> costs) -> bool {
             return heuristic.NextSolution(
                 VectorIntToVectorSubsetIndex(focus),
                 VectorDoubleToSubsetCostVector(costs));
           });

  py::class_<ElementDegreeSolutionGenerator>(m,
                                             "ElementDegreeSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](ElementDegreeSolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("next_solution",
           [](ElementDegreeSolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus));
           })
      .def("next_solution",
           [](ElementDegreeSolutionGenerator& heuristic,
              const std::vector<BaseInt>& focus,
              const std::vector<double>& costs) -> bool {
             return heuristic.NextSolution(
                 VectorIntToVectorSubsetIndex(focus),
                 VectorDoubleToSubsetCostVector(costs));
           });

  py::class_<LazyElementDegreeSolutionGenerator>(
      m, "LazyElementDegreeSolutionGenerator")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](LazyElementDegreeSolutionGenerator& heuristic) -> bool {
             return heuristic.NextSolution();
           })
      .def("next_solution",
           [](LazyElementDegreeSolutionGenerator& heuristic,
              const std::vector<BaseInt>& focus) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus));
           })
      .def("next_solution",
           [](LazyElementDegreeSolutionGenerator& heuristic,
              absl::Span<const BaseInt> focus,
              absl::Span<const double> costs) -> bool {
             return heuristic.NextSolution(
                 VectorIntToVectorSubsetIndex(focus),
                 VectorDoubleToSubsetCostVector(costs));
           });

  py::class_<SteepestSearch>(m, "SteepestSearch")
      .def(py::init<SetCoverInvariant*>())
      .def("next_solution",
           [](SteepestSearch& heuristic, int num_iterations) -> bool {
             return heuristic.NextSolution(num_iterations);
           })
      .def("next_solution",
           [](SteepestSearch& heuristic, const std::vector<BaseInt>& focus,
              int num_iterations) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus),
                                           num_iterations);
           })
      .def("next_solution",
           [](SteepestSearch& heuristic, const std::vector<BaseInt>& focus,
              const std::vector<double>& costs, int num_iterations) -> bool {
             return heuristic.NextSolution(
                 VectorIntToVectorSubsetIndex(focus),
                 VectorDoubleToSubsetCostVector(costs), num_iterations);
           });

  py::class_<GuidedLocalSearch>(m, "GuidedLocalSearch")
      .def(py::init<SetCoverInvariant*>())
      .def("initialize", &GuidedLocalSearch::Initialize)
      .def("next_solution",
           [](GuidedLocalSearch& heuristic, int num_iterations) -> bool {
             return heuristic.NextSolution(num_iterations);
           })
      .def("next_solution",
           [](GuidedLocalSearch& heuristic, absl::Span<const BaseInt> focus,
              int num_iterations) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus),
                                           num_iterations);
           });

  // Specialization for T = SubsetIndex ~= BaseInt (aka int for Python, whatever
  // the size of BaseInt).
  // A base type doesn't work, because TabuList uses `T::value` in the
  // constructor.
  py::class_<TabuList<SubsetIndex>>(m, "TabuList")
      .def(py::init([](int size) -> TabuList<SubsetIndex>* {
             return new TabuList<SubsetIndex>(SubsetIndex(size));
           }),
           arg("size"))
      .def("size", &TabuList<SubsetIndex>::size)
      .def("init", &TabuList<SubsetIndex>::Init, arg("size"))
      .def(
          "add",
          [](TabuList<SubsetIndex>& list, BaseInt t) -> void {
            return list.Add(SubsetIndex(t));
          },
          arg("t"))
      .def(
          "contains",
          [](TabuList<SubsetIndex>& list, BaseInt t) -> bool {
            return list.Contains(SubsetIndex(t));
          },
          arg("t"));

  py::class_<GuidedTabuSearch>(m, "GuidedTabuSearch")
      .def(py::init<SetCoverInvariant*>())
      .def("initialize", &GuidedTabuSearch::Initialize)
      .def("next_solution",
           [](GuidedTabuSearch& heuristic, int num_iterations) -> bool {
             return heuristic.NextSolution(num_iterations);
           })
      .def("next_solution",
           [](GuidedTabuSearch& heuristic, const std::vector<BaseInt>& focus,
              int num_iterations) -> bool {
             return heuristic.NextSolution(VectorIntToVectorSubsetIndex(focus),
                                           num_iterations);
           })
      .def("get_lagrangian_factor", &GuidedTabuSearch::SetLagrangianFactor,
           arg("factor"))
      .def("set_lagrangian_factor", &GuidedTabuSearch::GetLagrangianFactor)
      .def("set_epsilon", &GuidedTabuSearch::SetEpsilon, arg("r"))
      .def("get_epsilon", &GuidedTabuSearch::GetEpsilon)
      .def("set_penalty_factor", &GuidedTabuSearch::SetPenaltyFactor,
           arg("factor"))
      .def("get_penalty_factor", &GuidedTabuSearch::GetPenaltyFactor)
      .def("set_tabu_list_size", &GuidedTabuSearch::SetTabuListSize,
           arg("size"))
      .def("get_tabu_list_size", &GuidedTabuSearch::GetTabuListSize);

  m.def(
      "clear_random_subsets",
      [](BaseInt num_subsets, SetCoverInvariant* inv) -> std::vector<BaseInt> {
        const std::vector<SubsetIndex> cleared =
            ClearRandomSubsets(num_subsets, inv);
        return {cleared.begin(), cleared.end()};
      });
  m.def("clear_random_subsets",
        [](const std::vector<BaseInt>& focus, BaseInt num_subsets,
           SetCoverInvariant* inv) -> std::vector<BaseInt> {
          const std::vector<SubsetIndex> cleared = ClearRandomSubsets(
              VectorIntToVectorSubsetIndex(focus), num_subsets, inv);
          return {cleared.begin(), cleared.end()};
        });

  m.def(
      "clear_most_covered_elements",
      [](BaseInt num_subsets, SetCoverInvariant* inv) -> std::vector<BaseInt> {
        const std::vector<SubsetIndex> cleared =
            ClearMostCoveredElements(num_subsets, inv);
        return {cleared.begin(), cleared.end()};
      });
  m.def("clear_most_covered_elements",
        [](absl::Span<const BaseInt> focus, BaseInt num_subsets,
           SetCoverInvariant* inv) -> std::vector<BaseInt> {
          const std::vector<SubsetIndex> cleared = ClearMostCoveredElements(
              VectorIntToVectorSubsetIndex(focus), num_subsets, inv);
          return {cleared.begin(), cleared.end()};
        });

  // set_cover_reader.h
  m.def("read_orlib_scp", &ReadOrlibScp);
  m.def("read_orlib_rail", &ReadOrlibRail);
  m.def("read_fimi_dat", &ReadFimiDat);
  m.def("read_set_cover_proto", &ReadSetCoverProto);
  m.def("write_orlib_scp", &WriteOrlibScp);
  m.def("write_orlib_rail", &WriteOrlibRail);
  m.def("write_set_cover_proto", &WriteSetCoverProto);
  m.def("write_set_cover_solution_text", &WriteSetCoverSolutionText);
  m.def("write_set_cover_solution_proto", &WriteSetCoverSolutionProto);
  m.def("read_set_cover_solution_text", &ReadSetCoverSolutionText);
  m.def("read_set_cover_solution_proto", &ReadSetCoverSolutionProto);

  // set_cover_lagrangian.h
  // TODO(user): add support for SetCoverLagrangian.
}
