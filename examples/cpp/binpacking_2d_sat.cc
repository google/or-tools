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

// This file solves a 2D Bin Packing problem.
// It loads the size of the main rectangle, all available items (rectangles
// too), and tries to fit all rectangles in the minimum numbers of bins (they
// have the size of the main rectangle.)

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/path.h"
#include "ortools/packing/binpacking_2d_parser.h"
#include "ortools/packing/multiple_dimensions_bin_packing.pb.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"

ABSL_FLAG(std::string, input, "", "Input file.");
ABSL_FLAG(int, instance, -1, "Instance number if the file.");
ABSL_FLAG(std::string, params, "", "Sat parameters in text proto format.");
ABSL_FLAG(int, max_bins, 0,
          "Maximum number of bins. The 0 default value implies the code will "
          "use some heuristics to compute this number.");
ABSL_FLAG(int, symmetry_breaking_level, 2, "Use symmetry breaking constraints");
ABSL_FLAG(bool, use_global_cumulative, true,
          "Use a global cumulative relaxation");

namespace operations_research {
namespace sat {

namespace {

class GreaterByArea {
 public:
  explicit GreaterByArea(
      const packing::MultipleDimensionsBinPackingProblem& problem)
      : problem_(problem) {}

  bool operator()(int a, int b) const {
    const auto& a_sizes = problem_.items(a).shapes(0).dimensions();
    const auto& b_sizes = problem_.items(b).shapes(0).dimensions();

    return a_sizes.Get(0) * a_sizes.Get(1) > b_sizes.Get(0) * b_sizes.Get(1);
  }

 private:
  const packing::MultipleDimensionsBinPackingProblem& problem_;
};

bool ItemsAreIncompatible(
    const packing::MultipleDimensionsBinPackingProblem& problem, int i1,
    int i2) {
  const auto& bin_sizes = problem.box_shape().dimensions();
  const auto& i1_sizes = problem.items(i1).shapes(0).dimensions();
  const auto& i2_sizes = problem.items(i2).shapes(0).dimensions();

  return (i1_sizes.Get(0) + i2_sizes.Get(0) > bin_sizes[0]) &&
         (i1_sizes.Get(1) + i2_sizes.Get(1) > bin_sizes[1]);
}

absl::btree_set<int> FindFixedItems(
    const packing::MultipleDimensionsBinPackingProblem& problem) {
  absl::btree_set<int> fixed_items;

  // We start by fixing big pairwise incompatible items. Each to its own bin.
  // See Côté; Haouari; Iori. (2019). A Primal Decomposition Algorithm for the
  // Two-dimensional Bin Packing Problem (https://arxiv.org/pdf/1909.06835.pdf).
  const int num_items = problem.items_size();
  const auto bin_sizes = problem.box_shape().dimensions();

  for (int i = 0; i < num_items; ++i) {
    if (2 * problem.items(i).shapes(0).dimensions(0) > bin_sizes[0] &&
        2 * problem.items(i).shapes(0).dimensions(1) > bin_sizes[1]) {
      // Big items are pairwise incompatible. Just fix them in different bins.
      fixed_items.insert(i);
    }
  }

  // Now we fixed all items that are too big to fit any two of them in a bin.
  // There could still be two items that are incompatible with all the big ones
  // and one with one another: a very wide one and a very tall one. Let's fix
  // those two too if they exist. Note that if there are no big items
  // incompatible_pair_candidates contains all items and we will fix the first
  // pairwise incompatible pair.
  absl::btree_set<int> incompatible_pair_candidates;
  for (int i = 0; i < num_items; ++i) {
    if (fixed_items.contains(i)) {
      continue;
    }

    bool incompatible_with_all = true;
    for (int item : fixed_items) {
      if (!ItemsAreIncompatible(problem, item, i)) {
        incompatible_with_all = false;
        break;
      }
    }
    if (incompatible_with_all) {
      incompatible_pair_candidates.insert(i);
    }
  }
  bool found_incompatible_pair = false;
  for (const int i1 : incompatible_pair_candidates) {
    for (const int i2 : incompatible_pair_candidates) {
      if (i1 == i2) {
        continue;
      }
      if (ItemsAreIncompatible(problem, i1, i2)) {
        // We found a pair that is incompatible with all the big items and
        // between one another.
        fixed_items.insert(i1);
        fixed_items.insert(i2);
        found_incompatible_pair = true;
        break;
      }
    }
    if (found_incompatible_pair) {
      break;
    }
  }

  if (!found_incompatible_pair && !incompatible_pair_candidates.empty()) {
    // We could not add a pair of mutually incompatible items to our list. But
    // we know a set of elements that are incompatible with all the big ones.
    // Let's add the one with the largest area. Note that if there are no big
    // items, incompatible_pair_candidates contains all items and we will just
    // fix the largest element.
    fixed_items.insert(*std::min_element(incompatible_pair_candidates.begin(),
                                         incompatible_pair_candidates.end(),
                                         GreaterByArea(problem)));
  }

  if (fixed_items.size() > 1) {
    std::string_view message_end = ".";
    if (found_incompatible_pair) {
      message_end =
          " (including the extra two that are big in only one "
          "dimensions).";
    } else if (!incompatible_pair_candidates.empty()) {
      message_end =
          " (including an extra one that is incompatible with all big ones).";
    }
    LOG(INFO) << fixed_items.size() << " items are pairwise incompatible"
              << message_end;
  }

  return fixed_items;
}

// Solves a subset sum problem to find the maximum reachable max size.
int64_t MaxSubsetSumSize(absl::Span<const int64_t> sizes, int64_t max_size) {
  CpModelBuilder builder;
  LinearExpr weighed_sum;
  for (const int size : sizes) {
    weighed_sum += size * builder.NewBoolVar();
  }

  builder.AddLessOrEqual(weighed_sum, max_size);
  builder.Maximize(weighed_sum);

  const CpSolverResponse response = Solve(builder.Build());
  CHECK_EQ(response.status(), CpSolverStatus::OPTIMAL);
  return static_cast<int64_t>(response.objective_value());
}

}  // namespace

// Load a 2D bin packing problem and solve it.
void LoadAndSolve(const std::string& file_name, int instance) {
  packing::BinPacking2dParser parser;
  if (!parser.Load2BPFile(file_name, instance)) {
    LOG(FATAL) << "Cannot read instance " << instance << " from file '"
               << file_name << "'";
  }
  packing::MultipleDimensionsBinPackingProblem problem = parser.problem();
  LOG(INFO) << "Successfully loaded instance " << instance << " from file '"
            << file_name << "'";
  LOG(INFO) << "Instance has " << problem.items_size() << " items";

  const auto original_bin_sizes = problem.box_shape().dimensions();
  const int num_dimensions = original_bin_sizes.size();
  const int num_items = problem.items_size();

  // Non overlapping.
  if (num_dimensions == 1) {
    LOG(FATAL) << "One dimension is not supported.";
  } else if (num_dimensions != 2) {
    LOG(FATAL) << num_dimensions << " dimensions not supported.";
  }

  // Reduce the size of the bin with subset-sum.
  //
  // Short correctness proof: For any solution, we can transform it so that each
  // item is packed to the bottom and left. That is, touch an item or the bin
  // border on these sides. In that case, we can see that there is a "path" from
  // the top item, only moving down via touching items, to the bottom edge. And
  // similarly from the right most item, moving left, to the left edge. So on
  // each coordinate, the maximum size must be expressible as an exact sum of
  // the item sizes.
  std::vector<int64_t> x_sizes;
  std::vector<int64_t> y_sizes;
  int64_t sum_of_items_area = 0;
  for (const auto& item : problem.items()) {
    CHECK_EQ(1, item.shapes_size());
    const auto& shape = item.shapes(0);
    CHECK_EQ(2, shape.dimensions_size());
    sum_of_items_area += shape.dimensions(0) * shape.dimensions(1);
    x_sizes.push_back(shape.dimensions(0));
    y_sizes.push_back(shape.dimensions(1));
  }
  std::vector<int64_t> bin_sizes(2);
  bin_sizes[0] = MaxSubsetSumSize(x_sizes, original_bin_sizes[0]);
  bin_sizes[1] = MaxSubsetSumSize(y_sizes, original_bin_sizes[1]);
  if (bin_sizes[0] == original_bin_sizes[0] &&
      bin_sizes[1] == original_bin_sizes[1]) {
    LOG(INFO) << "Box size: [" << bin_sizes[0] << " * " << bin_sizes[1] << "]";
  } else {
    LOG(INFO) << "Box size: [" << bin_sizes[0] << " * " << bin_sizes[1]
              << "] reduced from [" << original_bin_sizes[0] << " * "
              << original_bin_sizes[1] << "]";
  }

  const int64_t area_of_one_bin = bin_sizes[0] * bin_sizes[1];
  const int64_t trivial_lb =
      MathUtil::CeilOfRatio(sum_of_items_area, area_of_one_bin);
  LOG(INFO) << "Trivial lower bound of the number of bins = " << trivial_lb;
  const int max_bins = absl::GetFlag(FLAGS_max_bins) == 0
                           ? trivial_lb * 2
                           : absl::GetFlag(FLAGS_max_bins);
  if (absl::GetFlag(FLAGS_max_bins) == 0) {
    LOG(INFO) << "Setting max_bins to " << max_bins;
  }

  CpModelBuilder cp_model;
  cp_model.SetName(absl::StrCat(
      "binpacking_2d_", file::Stem(absl::GetFlag(FLAGS_input)), "_", instance));

  // We do not support multiple shapes per item.
  for (int item = 0; item < num_items; ++item) {
    const int num_shapes = problem.items(item).shapes_size();
    CHECK_EQ(1, num_shapes);
  }

  // Create one Boolean variable per item and per bin.
  std::vector<std::vector<BoolVar>> item_to_bin(num_items);
  for (int item = 0; item < num_items; ++item) {
    item_to_bin[item].resize(max_bins);
    for (int b = 0; b < max_bins; ++b) {
      item_to_bin[item][b] = cp_model.NewBoolVar();
    }
  }

  // Exactly one bin is selected for each item.
  for (int item = 0; item < num_items; ++item) {
    cp_model.AddExactlyOne(item_to_bin[item]);
  }

  const absl::btree_set<int> fixed_items = FindFixedItems(problem);

  // Fix the fixed_items to the first fixed_items.size() bins.
  CHECK_LE(fixed_items.size(), max_bins)
      << "Infeasible problem, increase max_bins";
  int count = 0;
  for (const int item : fixed_items) {
    cp_model.FixVariable(item_to_bin[item][count], true);
    ++count;
  }

  // Detect incompatible pairs of items and add conflict at the bin level.
  int num_incompatible_pairs = 0;
  for (int i1 = 0; i1 + 1 < num_items; ++i1) {
    for (int i2 = i1 + 1; i2 < num_items; ++i2) {
      if (fixed_items.contains(i1) && fixed_items.contains(i2)) {
        // Both are already fixed to different bins.
        continue;
      }
      if (!ItemsAreIncompatible(problem, i1, i2)) {
        continue;
      }
      num_incompatible_pairs++;
      for (int b = 0; b < max_bins; ++b) {
        cp_model.AddAtMostOne({item_to_bin[i1][b], item_to_bin[i2][b]});
      }
    }
  }
  if (num_incompatible_pairs > 0) {
    LOG(INFO) << num_incompatible_pairs << " incompatible pairs of items";
  }

  // Compute the min size of all items in each dimension.
  std::vector<int64_t> min_sizes_per_dimension = bin_sizes;
  for (int item = 0; item < num_items; ++item) {
    for (int dim = 0; dim < num_dimensions; ++dim) {
      min_sizes_per_dimension[dim] =
          std::min(min_sizes_per_dimension[dim],
                   problem.items(item).shapes(0).dimensions(dim));
    }
  }

  // Manages positions and sizes for each item.
  //
  // Creates the starts variables.
  std::vector<std::vector<IntVar>> starts_by_dimension(num_items);
  absl::btree_set<int> items_exclusive_in_at_least_one_dimension;
  for (int item = 0; item < num_items; ++item) {
    starts_by_dimension[item].resize(num_dimensions);
    for (int dim = 0; dim < num_dimensions; ++dim) {
      const int64_t bin_size = bin_sizes[dim];
      const int64_t item_size = problem.items(item).shapes(0).dimensions(dim);
      // For item fixed to a given bin, by symmetry, we can also assume it
      // is in the lower left corner.
      const int64_t start_max = fixed_items.contains(item)
                                    ? (bin_size - item_size + 1) / 2
                                    : bin_size - item_size;
      starts_by_dimension[item][dim] = cp_model.NewIntVar({0, start_max});

      const int64_t size = problem.items(item).shapes(0).dimensions(dim);
      if (size + min_sizes_per_dimension[dim] > bin_size) {
        items_exclusive_in_at_least_one_dimension.insert(item);
      }
    }
  }

  // Creates the optional interval variables, sharing the same IntVar.
  std::vector<std::vector<std::vector<IntervalVar>>>
      interval_by_item_bin_dimension(num_items);
  for (int item = 0; item < num_items; ++item) {
    interval_by_item_bin_dimension[item].resize(max_bins);
    for (int b = 0; b < max_bins; ++b) {
      interval_by_item_bin_dimension[item][b].resize(num_dimensions);
      for (int dim = 0; dim < num_dimensions; ++dim) {
        const int64_t size = problem.items(item).shapes(0).dimensions(dim);
        const IntVar start = starts_by_dimension[item][dim];
        interval_by_item_bin_dimension[item][b][dim] =
            cp_model.NewOptionalFixedSizeIntervalVar(start, size,
                                                     item_to_bin[item][b]);
      }
    }
  }

  if (!items_exclusive_in_at_least_one_dimension.empty()) {
    int num_items_fixed_in_corner = 0;
    int num_items_fixed_on_one_border = 0;
    for (const int item : items_exclusive_in_at_least_one_dimension) {
      for (int dim = 0; dim < num_dimensions; ++dim) {
        if (fixed_items.contains(item)) {
          // Since this item is alone on its line (respectively column) and
          // effectively divides the bin in two we can put it in one corner. For
          // example, for a horizontal long item, solutions where the long item
          // sits in the middle would mean that there is also a solution where
          // the long item is moved all the way to the bottom.
          cp_model.FixVariable(starts_by_dimension[item][dim], 0);
          if (dim == 0) ++num_items_fixed_in_corner;
        } else {
          // Since this item is alone on its line (respectively column), we can
          // fix it at the beginning of the line (respectively column). Because
          // this item can be in the same bin as a fixed item or another
          // exclusive item, we cannot fix it to the bottom left corner.
          const int64_t bin_size = bin_sizes[dim];
          const int64_t item_size =
              problem.items(item).shapes(0).dimensions(dim);
          if (item_size + min_sizes_per_dimension[dim] > bin_size) {
            cp_model.FixVariable(starts_by_dimension[item][dim], 0);
            ++num_items_fixed_on_one_border;
          }
        }
      }
    }
    LOG(INFO) << num_items_fixed_in_corner << " items fixed in one corner";
    LOG(INFO) << num_items_fixed_on_one_border << " items fixed on one border";
  }

  if (absl::GetFlag(FLAGS_symmetry_breaking_level) >= 2) {
    // Break symmetry of a permutation of identical items
    absl::btree_map<std::pair<int64_t, int64_t>, std::vector<int>>
        item_indexes_for_dimensions;
    for (int item = 0; item < num_items; ++item) {
      item_indexes_for_dimensions[{problem.items(item).shapes(0).dimensions(0),
                                   problem.items(item).shapes(0).dimensions(1)}]
          .push_back(item);
    }
    int num_identical_items = 0;
    for (const auto& [dim, item_indexes] : item_indexes_for_dimensions) {
      if (item_indexes.size() == 1) {
        continue;
      }
      ++num_identical_items;
      for (int i = 1; i < item_indexes.size(); ++i) {
        const IntVar prev_start_x = starts_by_dimension[item_indexes[i - 1]][0];
        const IntVar curr_start_x = starts_by_dimension[item_indexes[i]][0];

        const IntVar prev_start_y = starts_by_dimension[item_indexes[i - 1]][1];
        const IntVar curr_start_y = starts_by_dimension[item_indexes[i]][1];
        cp_model.AddLessOrEqual(prev_start_x * bin_sizes[1] + prev_start_y,
                                curr_start_x * bin_sizes[1] + curr_start_y);
      }
    }
    if (num_identical_items > 0) {
      LOG(INFO) << num_identical_items << " identical items";
    }
  }

  // Add non overlapping constraint.
  for (int b = 0; b < max_bins; ++b) {
    NoOverlap2DConstraint no_overlap_2d = cp_model.AddNoOverlap2D();
    for (int item = 0; item < num_items; ++item) {
      no_overlap_2d.AddRectangle(interval_by_item_bin_dimension[item][b][0],
                                 interval_by_item_bin_dimension[item][b][1]);
    }
  }

  // Objective variable.
  const IntVar obj = cp_model.NewIntVar({trivial_lb, max_bins});

  // Global cumulative.
  if (absl::GetFlag(FLAGS_use_global_cumulative)) {
    DCHECK_EQ(num_dimensions, 2);
    for (int dim = 0; dim < num_dimensions; ++dim) {
      const int other_size = bin_sizes[1 - dim];
      CumulativeConstraint cumul = cp_model.AddCumulative(obj * other_size);
      for (int item = 0; item < num_items; ++item) {
        const int size = problem.items(item).shapes(0).dimensions(dim);
        const int demand = problem.items(item).shapes(0).dimensions(1 - dim);
        cumul.AddDemand(cp_model.NewFixedSizeIntervalVar(
                            starts_by_dimension[item][dim], size),
                        demand);
      }
    }
  }

  // Maintain one Boolean variable per bin that indicates if the bin is used
  // or not.
  std::vector<BoolVar> bin_is_used(max_bins);
  for (int b = 0; b < max_bins; ++b) {
    bin_is_used[b] = cp_model.NewBoolVar();

    // Link bin_is_used[i] with the items in bin i.
    std::vector<BoolVar> all_items_in_bin;
    for (int item = 0; item < num_items; ++item) {
      cp_model.AddImplication(item_to_bin[item][b], bin_is_used[b]);
      all_items_in_bin.push_back(item_to_bin[item][b]);
    }
    cp_model.AddBoolOr(all_items_in_bin).OnlyEnforceIf(bin_is_used[b]);
  }

  // Objective definition.
  if (absl::GetFlag(FLAGS_symmetry_breaking_level) >= 1) {
    CHECK_GT(trivial_lb, 0);
    for (int b = trivial_lb; b < max_bins; ++b) {
      cp_model.AddGreaterOrEqual(obj, b + 1).OnlyEnforceIf(bin_is_used[b]);
      cp_model.AddImplication(bin_is_used[b], bin_is_used[b - 1]);
    }
  } else {
    LinearExpr num_used_bins = 0;
    for (int b = 0; b < max_bins; ++b) {
      num_used_bins += bin_is_used[b];
    }
    cp_model.AddGreaterOrEqual(obj, num_used_bins);
  }
  cp_model.Minimize(obj);

  if (absl::GetFlag(FLAGS_symmetry_breaking_level) >= 1) {
    // First sort the items not yet fixed by area.
    std::vector<int> not_placed_items;
    for (int item = 0; item < num_items; ++item) {
      if (!fixed_items.contains(item)) {
        not_placed_items.push_back(item);
      }
    }
    std::sort(not_placed_items.begin(), not_placed_items.end(),
              GreaterByArea(problem));

    if (absl::GetFlag(FLAGS_symmetry_breaking_level) >= 3) {
      // Symmetry breaking: bin i "greater or equal" bin i-1.
      int first_empty_bin = fixed_items.size();
      const int num_active_items =
          std::min(static_cast<int>(not_placed_items.size()), 60);
      LinearExpr previous_bin_expr;
      for (int b = first_empty_bin; b < max_bins; ++b) {
        LinearExpr curr_bin_expr = 0;
        for (int i = 0; i < num_active_items; ++i) {
          curr_bin_expr +=
              item_to_bin[not_placed_items[i]][b] * (int64_t{1} << i);
        }
        if (b > first_empty_bin) {
          cp_model.AddLessOrEqual(curr_bin_expr, previous_bin_expr);
        }
        previous_bin_expr = curr_bin_expr;
      }
    } else {
      // Symmetry breaking: i-th biggest item is in bin <= i for the first
      // max_bins items.
      int first_empty_bin = fixed_items.size();
      for (const int item : not_placed_items) {
        if (first_empty_bin + 1 >= max_bins) break;
        for (int b = first_empty_bin + 1; b < max_bins; ++b) {
          cp_model.FixVariable(item_to_bin[item][b], false);
        }
        ++first_empty_bin;
      }
    }
  }

  // Setup parameters.
  SatParameters parameters;
  parameters.set_log_search_progress(true);

  // Parse the --params flag.
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  // If number of workers is >= 16 and < 24, we prefer replacing
  // objective_lb_search by objective_shaving_search.
  if (parameters.num_workers() >= 16 && parameters.num_workers() < 24) {
    parameters.add_ignore_subsolvers("objective_lb_search");
    parameters.add_extra_subsolvers("objective_shaving");
  }

  // We rely on the solver default logging to log the number of bins.
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  if (absl::GetFlag(FLAGS_instance) == -1) {
    LOG(FATAL) << "Please supply a valid instance number with --instance=";
  }

  operations_research::sat::LoadAndSolve(absl::GetFlag(FLAGS_input),
                                         absl::GetFlag(FLAGS_instance));
  return EXIT_SUCCESS;
}
