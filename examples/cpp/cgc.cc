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

// Two-Dimensional Constrained Guillotine Cutting

#include "examples/cpp/cgc.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "examples/cpp/cgc_data.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "re2/re2.h"

namespace operations_research {

bool ConstrainedGuillotineCuttingData::LoadFromFile(
    const std::string& input_file) {
  std::string buffer;

  if (!file::GetContents(input_file, &buffer, file::Defaults()).ok()) {
    LOG(ERROR) << "Could not read from file " << input_file;
    return false;
  }
  const std::vector<std::string> lines =
      absl::StrSplit(buffer, '\n', absl::SkipEmpty());

  if (lines.empty()) {
    LOG(ERROR) << "Empty file: " << input_file;
    return false;
  }

  int num_pieces;
  if (!RE2::FullMatch(lines[0], "\\s*(\\d+)\\s*", &num_pieces)) {
    LOG(ERROR) << "Could not parse number of pieces";
    return false;
  }

  if (0 >= num_pieces) {
    LOG(ERROR) << "There are no pieces in the problem specification";
    return false;
  }

  if (lines.size() != num_pieces + 2) {
    LOG(ERROR) << "File: " << input_file << " does not respect the format";
    return false;
  }

  if (!RE2::FullMatch(lines[1], "\\s*(\\d+)\\s+(\\d+)\\s*", &root_length_,
                      &root_width_)) {
    LOG(ERROR) << "Could not parse the size of the main rectangle";
    return false;
  }

  pieces_.resize(num_pieces);
  for (int i = 0; i < num_pieces; ++i) {
    if (!RE2::FullMatch(lines[i + 2],
                        "\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*",
                        &pieces_[i].length, &pieces_[i].width,
                        &pieces_[i].max_appearances, &pieces_[i].value)) {
      LOG(ERROR) << "Could not parse piece on line " << lines[i + 2]
                 << ", line number " << i + 3;
      return false;
    }
  }

  return true;
}

namespace {

// Depending on the part that was cut, returns an IntVar representing
// if the cut that was made on the (index / 2 + 1) rectangle is
// a guillotine cut.
IntVar* IsAGuillotineCut(int index,
                         const std::vector<IntVar*>& size_currently_cut,
                         const std::vector<IntVar*>& size_not_cut,
                         const std::vector<IntVar*>& parent_index,
                         const std::vector<int>& pieces_size, Solver* solver) {
  CHECK(solver != nullptr);
  // The size of the cut must be >= 1 in order for the cut to be a valid one.
  IntVar* const condition_var =
      solver->MakeIsGreaterOrEqualCstVar(size_currently_cut[index], 1);

  // The part that is not cut should remain the same.
  IntVar* const same_uncut_size_as_sibling =
      solver->MakeIsEqualVar(size_not_cut[index], size_not_cut[index + 1]);

  // The part that is not cut should remain the same with the parent.
  IntVar* const same_uncut_size_as_parent = solver->MakeIsEqualVar(
      size_not_cut[index],
      solver->MakeElement(size_not_cut, parent_index[index / 2 + 1]));

  // We make a cut if the size of the cut matches at least one of the pieces.
  IntVar* const cut_equals_piece_size = solver->MakeIsEqualCstVar(
      solver->MakeElement(pieces_size, size_currently_cut[index]), 1);

  // The sum of the sizes that were cut should equal the parent size.
  IntVar* const sum_of_sizes = solver->MakeIsEqualVar(
      solver->MakeSum(size_currently_cut[index], size_currently_cut[index + 1]),
      solver->MakeElement(size_currently_cut, parent_index[index / 2 + 1]));

  const std::vector<IntVar*> cut_implications = {
      same_uncut_size_as_sibling, same_uncut_size_as_parent,
      cut_equals_piece_size, sum_of_sizes};

  return solver
      ->MakeConditionalExpression(
          condition_var,
          solver->MakeIsEqualCstVar(solver->MakeSum(cut_implications),
                                    cut_implications.size()),
          0)
      ->Var();
}

// Sets initial variables:
//  stores piece sizes related variables and sets maximum value
//  and maximum number of elements in a cutting path.
void SetInitialElements(const ConstrainedGuillotineCuttingData& data,
                        std::vector<int>* sizes_to_pieces,
                        std::vector<int>* piece_length,
                        std::vector<int>* piece_width, int* maximum_value,
                        int* maximum_elements) {
  CHECK(sizes_to_pieces != nullptr);
  CHECK(piece_length != nullptr);
  CHECK(piece_width != nullptr);
  CHECK(maximum_value != nullptr);
  CHECK(maximum_elements != nullptr);
  sizes_to_pieces->resize((data.root_length() + 1) * (data.root_width() + 1),
                          data.pieces().size());
  piece_length->resize(data.root_length() + 1, 0);
  piece_width->resize(data.root_width() + 1, 0);

  const int main_rectangle_area = data.root_length() * data.root_width();

  // Number of elements in the path should dependent on the number
  // of end pieces. Considering that at every point we could in 2 cuts
  // get to an end piece, which means maximum 4 new pieces,
  // a limit of 4 * number_of_end_pieces should fit the path.
  static const int kMultiplyNumOfEndPiecesBy = 4;
  *maximum_value = 0;
  *maximum_elements = 1;

  int index = 0;
  for (const ConstrainedGuillotineCuttingData::Piece& piece : data.pieces()) {
    if (piece.length <= data.root_length() &&
        piece.width <= data.root_width()) {
      (*sizes_to_pieces)[piece.length * data.root_width() + piece.width] =
          index;
      (*piece_length)[piece.length] = 1;
      (*piece_width)[piece.width] = 1;
    }

    const int number_of_appearances =
        std::min(piece.max_appearances,
                 main_rectangle_area / (piece.length * piece.width));

    *maximum_value += piece.value * number_of_appearances;
    *maximum_elements += kMultiplyNumOfEndPiecesBy * number_of_appearances;
    index++;
  }

  // TODO(user): a better upper bound for value and for
  // maximum_elements.
}

void SetRectanglesVariablesAndAddConstraints(
    const std::vector<int>& piece_length, const std::vector<int>& piece_width,
    const int maximum_elements, const int root_length, const int root_width,
    std::vector<IntVar*>* parent_index, std::vector<IntVar*>* rectangle_length,
    std::vector<IntVar*>* rectangle_width, Solver* solver) {
  CHECK(parent_index != nullptr);
  CHECK(rectangle_length != nullptr);
  CHECK(rectangle_width != nullptr);
  CHECK(solver != nullptr);

  static const int kMainRectangleIndex = 0;

  solver->MakeIntVarArray(maximum_elements / 2 + 2, -1, maximum_elements,
                          "parent_index_", parent_index);
  solver->MakeIntVarArray(maximum_elements, 0, root_length, "length_",
                          rectangle_length);
  solver->MakeIntVarArray(maximum_elements, 0, root_width, "width_",
                          rectangle_width);

  (*parent_index)[kMainRectangleIndex]->SetValue(-1);
  (*rectangle_length)[kMainRectangleIndex]->SetValue(root_length);
  (*rectangle_width)[kMainRectangleIndex]->SetValue(root_width);

  // Any rectangle can be cut just once.
  solver->AddConstraint(solver->MakeAllDifferent(*parent_index));

  std::vector<IntVar*> x_guillotine_cut;
  std::vector<IntVar*> y_guillotine_cut;

  // Every 2 consecutive cuts are from the same rectangle starting with
  // position 1, since at index 0 we keep information regarding the
  // main rectangle.
  for (int i = 1; i < maximum_elements; i += 2) {
    // The rectangle from which we cut needs to be < i. In case we do not cut
    // anything (the elements are all 0) the parent_index will be i.
    solver->AddConstraint(
        solver->MakeLessOrEqual((*parent_index)[i / 2 + 1], i));

    // If one of the sizes is 0, then all are 0 and the parent does not point
    // to a real parent, but to itself, since we cannot have a valid cut
    // that leaves one size 0.
    IntVar* length_is_zero =
        solver->MakeIsEqualCstVar((*rectangle_length)[i], 0);

    solver->AddConstraint(solver->MakeEquality(
        length_is_zero,
        solver->MakeIsEqualCstVar((*rectangle_length)[i + 1], 0)));

    solver->AddConstraint(solver->MakeEquality(
        length_is_zero, solver->MakeIsEqualCstVar((*rectangle_width)[i], 0)));

    solver->AddConstraint(solver->MakeEquality(
        length_is_zero,
        solver->MakeIsEqualCstVar((*rectangle_width)[i + 1], 0)));

    solver->AddConstraint(solver->MakeEquality(
        length_is_zero,
        solver->MakeIsEqualCstVar((*parent_index)[i / 2 + 1], i)));

    // Group 0-cuts together at the beginning. So after a normal cut there
    // will not be any 0-cuts.
    if (i > 1) {
      solver->AddConstraint(solver->MakeLessOrEqual(
          solver->MakeIsGreaterOrEqualCstVar((*rectangle_length)[i - 1], 1),
          solver->MakeIsGreaterOrEqualCstVar((*rectangle_length)[i], 1)));
    }

    // If it is an x-guillotine cut.
    x_guillotine_cut.push_back(IsAGuillotineCut(i, *rectangle_length,
                                                *rectangle_width, *parent_index,
                                                piece_length, solver));

    // If it is an y-guillotine cut.
    y_guillotine_cut.push_back(
        IsAGuillotineCut(i, *rectangle_width, *rectangle_length, *parent_index,
                         piece_width, solver));

    // Every pair of rectangles should correspond to a guillotine cut
    // on one of the axis or they could be 0 if there was no cut made.
    solver->AddConstraint(solver->MakeEquality(
        solver->MakeSum(
            length_is_zero,
            solver->MakeSum(
                solver->MakeIsEqualCstVar(x_guillotine_cut[i / 2], 1),
                solver->MakeIsEqualCstVar(y_guillotine_cut[i / 2], 1))),
        1));
  }
}

void AddAdditionalConstraints(const std::vector<IntVar*>& parent_index,
                              const std::vector<IntVar*>& rectangle_length,
                              const std::vector<IntVar*>& rectangle_width,
                              const std::vector<int>& sizes_to_pieces,
                              const ConstrainedGuillotineCuttingData& data,
                              int maximum_elements,
                              std::vector<IntVar*>* is_end_piece,
                              std::vector<IntVar*>* was_cut, IntVar* value,
                              Solver* solver) {
  CHECK(is_end_piece != nullptr);
  CHECK(was_cut != nullptr);
  CHECK(value != nullptr);
  CHECK(solver != nullptr);
  solver->MakeIntVarArray(maximum_elements, 0, 1, "", was_cut);

  for (int i = 0; i < maximum_elements; ++i) {
    solver->AddConstraint(solver->MakeCount(parent_index, i, (*was_cut)[i]));

    is_end_piece->push_back(
        solver
            ->MakeConditionalExpression(
                solver->MakeIsEqualCstVar((*was_cut)[i], 0),
                solver->MakeElement(
                    sizes_to_pieces,
                    solver
                        ->MakeSum(solver->MakeProd(rectangle_length[i],
                                                   data.root_width()),
                                  rectangle_width[i])
                        ->Var()),
                data.pieces().size())
            ->Var());
  }

  int index = 0;
  std::vector<IntVar*> values;

  for (const ConstrainedGuillotineCuttingData::Piece& piece : data.pieces()) {
    // Number of appearances of every type should be less or equal
    // to the maximum number of times a piece can appear.
    IntVar* const appearances =
        solver->MakeIntVar(0, (data.root_length() * data.root_width()) /
                                  (piece.length * piece.width));

    // The number of appearances of every piece should be equal
    // to the number of times that piece appears in a path as an end piece.
    solver->AddConstraint(solver->MakeCount(*is_end_piece, index, appearances));

    // Values will contain for every piece:
    // number_of_time_the_piece_appears * its value.
    values.push_back(
        solver
            ->MakeProd(solver->MakeMin(appearances, piece.max_appearances),
                       piece.value)
            ->Var());

    index++;
  }

  solver->AddConstraint(solver->MakeEquality(value, solver->MakeSum(values)));
}

void CreateAdditionalMonitors(absl::Duration time_limit,
                              std::vector<SearchMonitor*>* monitors,
                              OptimizeVar* objective_value, Solver* solver) {
  CHECK(monitors != nullptr);
  CHECK(objective_value != nullptr);
  CHECK(solver != nullptr);
  monitors->push_back(objective_value);

  static const int kLogFrequency = 100000;
  SearchMonitor* const log =
      solver->MakeSearchLog(kLogFrequency, objective_value);
  monitors->push_back(log);

  if (time_limit != absl::InfiniteDuration()) {
    SearchLimit* const limit = solver->MakeTimeLimit(time_limit);
    monitors->push_back(limit);
  }
}

DecisionBuilder* CreateDecisionBuilder(
    const std::vector<IntVar*>& parent_index,
    const std::vector<IntVar*>& rectangle_length,
    const std::vector<IntVar*>& rectangle_width,
    const std::vector<IntVar*>& was_cut, Solver* solver) {
  CHECK(solver != nullptr);
  std::vector<IntVar*> decision_variables;
  for (int i = 1; i < rectangle_length.size() / 2 + 1; ++i) {
    decision_variables.push_back(parent_index[i]);
    decision_variables.push_back(rectangle_length[2 * (i - 1) + 1]);
    decision_variables.push_back(rectangle_width[2 * (i - 1) + 1]);
  }
  for (int i = 0; i < rectangle_length.size(); ++i) {
    decision_variables.push_back(was_cut[i]);
  }

  return solver->MakePhase(decision_variables, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MAX_VALUE);
}

void FillSolution(
    const std::vector<IntVar*>& parent_index,
    const std::vector<IntVar*>& rectangle_length,
    const std::vector<IntVar*>& rectangle_width,
    const SolutionCollector* collector, IntVar* value, int* maximum_value,
    std::vector<ConstrainedGuillotineCutting::CutRectangle>* solution) {
  CHECK(collector != nullptr);
  CHECK(value != nullptr);
  CHECK(maximum_value != nullptr);
  CHECK(solution != nullptr);

  int number_of_zero_cuts = 0;
  int parent = -1;
  for (int i = 0; i < rectangle_length.size(); ++i) {
    if (!collector->Value(0, rectangle_length[i])) {
      number_of_zero_cuts++;
      continue;
    }

    if (i % 2 == 1) {
      parent = std::max(
          collector->Value(0, parent_index[i / 2 + 1]) - number_of_zero_cuts,
          int64_t{0});
    }

    solution->emplace_back(parent, collector->Value(0, rectangle_length[i]),
                           collector->Value(0, rectangle_width[i]));
  }

  *maximum_value = collector->Value(0, value);
}

void ValidateSolution(int num_pieces, int root_width,
                      const std::vector<IntVar*>& parent_index,
                      const std::vector<IntVar*>& rectangle_length,
                      const std::vector<IntVar*>& rectangle_width,
                      const std::vector<IntVar*>& is_end_piece,
                      absl::Span<const int> sizes_to_pieces,
                      const SolutionCollector* collector) {
  CHECK(collector != nullptr);
  absl::btree_set<int> parent_ids;
  for (int i = 0; i < parent_index.size(); ++i) {
    parent_ids.insert(collector->Value(0, parent_index[i]));
    // The rectangle from which the rectangles were cut needs to be
    // <= current position. For every pair of rectangles we keep
    // their parent index once.
    CHECK(collector->Value(0, parent_index[i]) <= i * 2 - 1);
  }
  // Every piece should be cut just once.
  CHECK_EQ(parent_ids.size(), parent_index.size());

  bool guillotine_cut = false;
  for (int i = 1; i < rectangle_length.size(); i += 2) {
    const int parent = collector->Value(0, parent_index[i / 2 + 1]);
    const int length_left_rectangle = collector->Value(0, rectangle_length[i]);
    const int length_rigth_rectangle =
        collector->Value(0, rectangle_length[i + 1]);
    const int length_parent = collector->Value(0, rectangle_length[parent]);
    const int width_parent = collector->Value(0, rectangle_width[parent]);
    const int width_left_rectangle = collector->Value(0, rectangle_width[i]);
    const int width_right_rectangle =
        collector->Value(0, rectangle_width[i + 1]);

    const bool is_a_x_guillotine_cut =
        length_left_rectangle + length_rigth_rectangle == length_parent &&
        length_left_rectangle && length_rigth_rectangle &&
        width_left_rectangle == width_right_rectangle &&
        width_left_rectangle == width_parent;

    const bool is_a_y_guillotine_cut =
        width_left_rectangle + width_right_rectangle == width_parent &&
        width_left_rectangle && width_right_rectangle &&
        length_left_rectangle == length_rigth_rectangle &&
        length_left_rectangle == length_parent;

    const bool is_a_zero_cut = !length_left_rectangle &&
                               !length_rigth_rectangle &&
                               !width_left_rectangle && !width_right_rectangle;

    // Every cut is a guillotine cut or all elements are 0.
    CHECK(is_a_x_guillotine_cut || is_a_y_guillotine_cut || is_a_zero_cut);

    // Check if it is a piece.
    const int it_is_piece1 =
        parent_ids.contains(i)
            ? num_pieces
            : sizes_to_pieces[length_left_rectangle * root_width +
                              width_left_rectangle];

    const int it_is_piece2 =
        parent_ids.contains(i + 1)
            ? num_pieces
            : sizes_to_pieces[length_rigth_rectangle * root_width +
                              width_right_rectangle];

    CHECK_EQ(it_is_piece1, collector->Value(0, is_end_piece[i]));
    CHECK_EQ(it_is_piece2, collector->Value(0, is_end_piece[i + 1]));

    // Check that all 0-cuts (both rectangles are 0x0) are grouped together.
    CHECK_LE(guillotine_cut, is_a_x_guillotine_cut || is_a_y_guillotine_cut);
    guillotine_cut |= is_a_x_guillotine_cut || is_a_y_guillotine_cut;
  }
}

}  // namespace

void ConstrainedGuillotineCutting::PrintSolution() const {
  CHECK(solved_);

  absl::PrintF("Maximum value: %d\n", maximum_value_);
  absl::PrintF("Main rectangle 0 sizes: %dx%d\n", data_->root_length(),
               data_->root_width());
  for (int i = 1; i < solution_.size(); ++i) {
    if (i % 2 == 1) {
      absl::PrintF("\nRectangle %d was cut in: \n", solution_[i].parent_index);
    }
    absl::PrintF("Rectangle %d sizes: %dx%d\n", i, solution_[i].length,
                 solution_[i].width);
  }
}

void ConstrainedGuillotineCutting::Solve(absl::Duration time_limit) {
  const std::vector<ConstrainedGuillotineCuttingData::Piece>& pieces =
      data_->pieces();

  // Depending on the size of a rectangle, it represents the index of
  // the piece to which it corresponds. If it does not correspond to
  // any piece, than it will remain pieces.size().
  std::vector<int> sizes_to_pieces;

  // Depending on the length(width) of a rectangle, it will be 1 if
  // there exists a piece that has that length(width).
  std::vector<int> piece_length;
  std::vector<int> piece_width;

  int maximum_value;
  int maximum_elements;
  SetInitialElements(*data_, &sizes_to_pieces, &piece_length, &piece_width,
                     &maximum_value, &maximum_elements);

  // For every pair of rectangles the index of the rectangle
  // that the rectangles were cut of.
  std::vector<IntVar*> parent_index;

  // sizes of the rectangles
  std::vector<IntVar*> rectangle_length;
  std::vector<IntVar*> rectangle_width;
  SetRectanglesVariablesAndAddConstraints(
      piece_length, piece_width, maximum_elements, data_->root_length(),
      data_->root_width(), &parent_index, &rectangle_length, &rectangle_width,
      &solver_);

  // Contains the piece that this rectangle equals to if it is
  // an end piece (it was not cut).
  std::vector<IntVar*> is_end_piece;
  // For every piece it is true if the corresponding rectangle
  // was cut.
  std::vector<IntVar*> was_cut;
  IntVar* const value = solver_.MakeIntVar(0, maximum_value);
  AddAdditionalConstraints(parent_index, rectangle_length, rectangle_width,
                           sizes_to_pieces, *data_, maximum_elements,
                           &is_end_piece, &was_cut, value, &solver_);
  // Objective: maximize the value of the end pieces.
  OptimizeVar* const objective_value = solver_.MakeMaximize(value, 1);

  DecisionBuilder* const db = CreateDecisionBuilder(
      parent_index, rectangle_length, rectangle_width, was_cut, &solver_);
  std::vector<SearchMonitor*> monitors;

  SolutionCollector* const collector = solver_.MakeLastSolutionCollector();
  collector->Add(parent_index);
  collector->Add(rectangle_length);
  collector->Add(rectangle_width);
  collector->Add(is_end_piece);
  collector->Add(value);
  monitors.push_back(collector);

  CreateAdditionalMonitors(time_limit, &monitors, objective_value, &solver_);

  const int64_t start_time = solver_.wall_time();
  solver_.Solve(db, monitors);
  const int64_t end_time = solver_.wall_time();

  LOG(INFO) << "The process took: " << (end_time - start_time) / 1000.0
            << " seconds.";

  if (collector->solution_count()) {
    ValidateSolution(pieces.size(), data_->root_width(), parent_index,
                     rectangle_length, rectangle_width, is_end_piece,
                     sizes_to_pieces, collector);

    solved_ = true;
    FillSolution(parent_index, rectangle_length, rectangle_width, collector,
                 value, &maximum_value_, &solution_);
  }
}

}  // namespace operations_research
