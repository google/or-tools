// Copyright 2010-2014 Google
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


// Demonstration of column generation using LP toolkit.
//
// Column generation is the technique of generating columns (aka
// resource bundles aka variables) of the constraint matrix
// incrementally guided by feedback from the constraint duals
// (cost-of-resources).  Frequently this lets one solve large problems
// efficiently, e.g. problems where the number of potential columns is
// exponentially large.
//
// Solves a covering problem taken from ITA Software recruiting web
// site:
//
// "Strawberries are growing in the cells of a rectangular field
// (grid). You want to build greenhouses to enclose the
// strawberries. Greenhouses are rectangular, axis-aligned with the
// field (i.e., not diagonal), and may not overlap. The cost of each
// greenhouse is $10 plus $1 per unit of area covered."
//
// Variables:
//
//    for each Box (greenhouse), continuous variable b{x1,y1,x2,y2} in [0,1]
//
// Constraints:
//
//   box limit:
//     sum b{x1,y1,x2,y2) <= MAX_BOXES
//   non-overlap (for each cell x,y):
//     sum b{x1,y1,x2,y2} <= 1     (summed over containing x1<=x<=x2, y1<=y<=y2)
//   coverage (for each cell x,y with a strawberry):
//     sum b{x1,y1,x2,y2} = 1      (summed over containing x1<=x<=x2, y1<=y<=y2)
//
// Since the number of possible boxes is O(d^4) where d is the linear
// dimension, starts from singleton column (box) covering entire grid,
// ensuring solvability.  Then iteratively the problem is solved and
// the constraint duals (aka reduced costs) used to guide the
// generation of a single new column (box), until convergence or a
// maximum number of iterations.
//
// No attempt is made to force integrality.

#include <cstdio>
#include <cstring>  // strlen
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/linear_solver/linear_solver.h"

DEFINE_bool(colgen_verbose, false, "print verbosely");
DEFINE_bool(colgen_complete, false, "generate all columns initially");
DEFINE_int32(colgen_max_iterations, 500, "max iterations");
DEFINE_string(colgen_solver, "glop", "solver - glop (default) or clp");
DEFINE_int32(colgen_instance, -1, "Which instance to solve (0 - 9)");

namespace operations_research {
// ---------- Data Instances ----------
struct Instance {
  int max_boxes;
  int width;
  int height;
  const char* grid;
};

Instance kInstances[] = {{4, 22, 6,
                          "..@@@@@..............."
                          "..@@@@@@........@@@..."
                          ".....@@@@@......@@@..."
                          ".......@@@@@@@@@@@@..."
                          ".........@@@@@........"
                          ".........@@@@@........"},
                         {3, 13, 10,
                          "............."
                          "............."
                          "............."
                          "...@@@@......"
                          "...@@@@......"
                          "...@@@@......"
                          ".......@@@..."
                          ".......@@@..."
                          ".......@@@..."
                          "............."},
                         {4, 13, 9,
                          "............."
                          "..@.@.@......"
                          "...@.@.@....."
                          "..@.@.@......"
                          "..@.@.@......"
                          "...@.@.@....."
                          "....@.@......"
                          "..........@@@"
                          "..........@@@"},
                         {4, 13, 9,
                          ".........@..."
                          ".........@..."
                          "@@@@@@@@@@..."
                          "..@......@..."
                          "..@......@..."
                          "..@......@..."
                          "..@@@@@@@@@@@"
                          "..@.........."
                          "..@.........."},
                         {7, 25, 14,
                          "........................."
                          "..@@@@@@@@@@@@@@@@@@@@..."
                          "..@@@@@@@@@@@@@@@@@@@@..."
                          "..@@.................@..."
                          "..@@.................@..."
                          "..@@.......@@@.......@.@."
                          "..@@.......@@@.......@..."
                          "..@@...@@@@@@@@@@@@@@@..."
                          "..@@...@@@@@@@@@@@@@@@..."
                          "..@@.......@@@.......@..."
                          "..@@.......@@@.......@..."
                          "..@@.................@..."
                          "..@@.................@..."
                          "........................."},
                         {6, 25, 16,
                          "........................."
                          "......@@@@@@@@@@@@@......"
                          "........................."
                          ".....@..........@........"
                          ".....@..........@........"
                          ".....@......@............"
                          ".....@......@.@@@@@@@...."
                          ".....@......@............"
                          ".....@......@.@@@@@@@...."
                          ".....@......@............"
                          "....@@@@....@............"
                          "....@@@@....@............"
                          "..@@@@@@....@............"
                          "..@@@.......@............"
                          "..@@@...................."
                          "..@@@@@@@@@@@@@@@@@@@@@@@"},
                         {5, 40, 18,
                          "........................................"
                          "........................................"
                          "...@@@@@@..............................."
                          "...@@@@@@..............................."
                          "...@@@@@@..............................."
                          "...@@@@@@.........@@@@@@@@@@............"
                          "...@@@@@@.........@@@@@@@@@@............"
                          "..................@@@@@@@@@@............"
                          "..................@@@@@@@@@@............"
                          ".............@@@@@@@@@@@@@@@............"
                          ".............@@@@@@@@@@@@@@@............"
                          "........@@@@@@@@@@@@...................."
                          "........@@@@@@@@@@@@...................."
                          "........@@@@@@.........................."
                          "........@@@@@@.........................."
                          "........................................"
                          "........................................"
                          "........................................"},
                         {8, 40, 18,
                          "........................................"
                          "..@@.@.@.@.............................."
                          "..@@.@.@.@...............@.............."
                          "..@@.@.@.@............@................."
                          "..@@.@.@.@.............................."
                          "..@@.@.@.@.................@............"
                          "..@@.@..................@..............."
                          "..@@.@.................................."
                          "..@@.@.................................."
                          "..@@.@................@@@@.............."
                          "..@@.@..............@@@@@@@@............"
                          "..@@.@.................................."
                          "..@@.@..............@@@@@@@@............"
                          "..@@.@.................................."
                          "..@@.@................@@@@.............."
                          "..@@.@.................................."
                          "..@@.@.................................."
                          "........................................"},
                         {10, 40, 19,
                          "@@@@@..................................."
                          "@@@@@..................................."
                          "@@@@@..................................."
                          "@@@@@..................................."
                          "@@@@@..................................."
                          "@@@@@...........@@@@@@@@@@@............."
                          "@@@@@...........@@@@@@@@@@@............."
                          "....................@@@@................"
                          "....................@@@@................"
                          "....................@@@@................"
                          "....................@@@@................"
                          "....................@@@@................"
                          "...............@@@@@@@@@@@@@@..........."
                          "...............@@@@@@@@@@@@@@..........."
                          ".......@@@@@@@@@@@@@@@@@@@@@@..........."
                          ".......@@@@@@@@@........................"
                          "........................................"
                          "........................................"
                          "........................................"},
                         {10, 40, 25,
                          "...................@...................."
                          "...............@@@@@@@@@................"
                          "............@@@.........@@@............."
                          "...........@...............@............"
                          "..........@.................@..........."
                          ".........@...................@.........."
                          ".........@...................@.........."
                          ".........@.....@@......@@....@.........."
                          "........@.....@@@@....@@@@....@........."
                          "........@.....................@........."
                          "........@.....................@........."
                          "........@..........@@.........@........."
                          ".......@@..........@@.........@@........"
                          "........@.....................@........."
                          "........@.....................@........."
                          "........@......@@@@@@@@@......@........."
                          "........@......@@@@@@@@@......@........."
                          ".........@...................@.........."
                          ".........@...................@.........."
                          ".........@...................@.........."
                          "..........@.................@..........."
                          "...........@...............@............"
                          "............@@@.........@@@............."
                          "...............@@@@@@@@@................"
                          "...................@...................."}};

const int kInstanceCount = 10;

// ---------- Box ---------

class Box {
 public:
  static const int kAreaCost = 1;
  static const int kFixedCost = 10;

  Box() {}
  Box(int x_min, int x_max, int y_min, int y_max)
      : x_min_(x_min), x_max_(x_max), y_min_(y_min), y_max_(y_max) {
    CHECK_GE(x_max, x_min);
    CHECK_GE(y_max, y_min);
  }

  int x_min() const { return x_min_; }
  int x_max() const { return x_max_; }
  int y_min() const { return y_min_; }
  int y_max() const { return y_max_; }

  // Lexicographic order
  int Compare(const Box& box) const {
    int c;
    if ((c = (x_min() - box.x_min())) != 0) return c;
    if ((c = (x_max() - box.x_max())) != 0) return c;
    if ((c = (y_min() - box.y_min())) != 0) return c;
    return y_max() - box.y_max();
  }

  bool Contains(int x, int y) const {
    return x >= x_min() && x <= x_max() && y >= y_min() && y <= y_max();
  }

  int Cost() const {
    return kAreaCost * (x_max() - x_min() + 1) * (y_max() - y_min() + 1) +
           kFixedCost;
  }

  std::string DebugString() const {
    return StringPrintf("[%d,%dx%d,%d]c%d", x_min(), y_min(), x_max(), y_max(),
                        Cost());
  }

 private:
  int x_min_;
  int x_max_;
  int y_min_;
  int y_max_;
};

struct BoxLessThan {
  bool operator()(const Box& b1, const Box& b2) const {
    return b1.Compare(b2) < 0;
  }
};

// ---------- Covering Problem ---------

class CoveringProblem {
 public:
  // Grid is a row-major std::string of length width*height with '@' for an
  // occupied cell (strawberry) and '.' for an empty cell.  Solver is
  // not owned.
  CoveringProblem(MPSolver* const solver, const Instance& instance)
      : solver_(solver),
        max_boxes_(instance.max_boxes),
        width_(instance.width),
        height_(instance.height),
        grid_(instance.grid) {}

  // Constructs initial variables and constraints.  Initial column
  // (box) covers entire grid, ensuring feasibility.
  bool Init() {
    // Check consistency.
    int size = strlen(grid_);
    if (size != area()) {
      return false;
    }
    for (int i = 0; i < size; ++i) {
      char c = grid_[i];
      if ((c != '@') && (c != '.')) return false;
    }

    AddCellConstraints();     // sum for every cell is <=1 or =1
    AddMaxBoxesConstraint();  // sum of box variables is <= max_boxes()
    if (!FLAGS_colgen_complete) {
      AddBox(Box(0, width() - 1, 0, height() - 1));  // grid-covering box
    } else {
      // Naive alternative to column generation - generate all boxes;
      // works fine for smaller problems, too slow for big.
      for (int y_min = 0; y_min < height(); ++y_min) {
        for (int y_max = y_min; y_max < height(); ++y_max) {
          for (int x_min = 0; x_min < width(); ++x_min) {
            for (int x_max = x_min; x_max < width(); ++x_max) {
              AddBox(Box(x_min, x_max, y_min, y_max));
            }
          }
        }
      }
    }
    return true;
  }

  int width() const { return width_; }
  int height() const { return height_; }
  int area() const { return width() * height(); }
  int max_boxes() const { return max_boxes_; }

  bool IsCellOccupied(int x, int y) const { return grid_[index(x, y)] == '@'; }

  // Calculates reduced costs for each possible Box and if any is
  // negative (improves cost), returns reduced cost and set target to
  // the most-negative (steepest descent) one - otherwise returns 0..
  //
  // For a problem in standard form 'minimize c*x s.t. Ax<=b, x>=0'
  // the reduced cost vector is c - transp(y) * A where y is the dual
  // cost column vector.
  //
  // For this covering problem, in which all coefficients in A are 0
  // or 1, this reduces to:
  //
  //   reduced_cost(box) =
  //
  //     box.Cost() - sum_{enclosed cell} cell_constraint->dual_value()
  //                - max_boxes_constraint_->dual_value()
  //
  // Since there are O(d^4) boxes, we don't also want O(d^2) sum for
  // each, so pre-calculate sums of cell duals for all rectangles with
  // upper-left at 0, 0, and use these to calculate the sum in
  // constant time using the standard inclusion-exclusion trick.
  double GetOptimalBox(Box* const target) {
    // Cost change threshold for new Box
    const double kCostChangeThreshold = -.01;

    // Precomputes the sum of reduced costs for every upper-left
    // rectangle.
    std::vector<double> upper_left_sums(area());
    ComputeUpperLeftSums(&upper_left_sums);

    const double max_boxes_dual = max_boxes_constraint_->dual_value();
    double best_reduced_cost = kCostChangeThreshold;
    Box best_box;
    for (int y_min = 0; y_min < height(); ++y_min) {
      for (int y_max = y_min; y_max < height(); ++y_max) {
        for (int x_min = 0; x_min < width(); ++x_min) {
          for (int x_max = x_min; x_max < width(); ++x_max) {
            Box box(x_min, x_max, y_min, y_max);
            const double cell_coverage_dual =  // inclusion-exclusion
                +zero_access(upper_left_sums, x_max, y_max) -
                zero_access(upper_left_sums, x_max, y_min - 1) -
                zero_access(upper_left_sums, x_min - 1, y_max) +
                zero_access(upper_left_sums, x_min - 1, y_min - 1);

            // All coefficients for new column are 1, so no need to
            // multiply constraint duals by any coefficients when
            // computing the reduced cost.
            const double reduced_cost =
                box.Cost() - (cell_coverage_dual + max_boxes_dual);

            if (reduced_cost < best_reduced_cost) {
              // Even with negative reduced cost, the box may already
              // exist, and even be basic (part of solution)!  This
              // counterintuitive situation is due to the problem's
              // many redundant linear equality constraints: many
              // steepest-edge pivot moves will be of zero-length.
              // Ideally one would want to check the length of the
              // move but that is difficult without access to the
              // internals of the solver (e.g., access to B^-1 in the
              // simplex algorithm).
              if (boxes_.find(box) == boxes_.end()) {
                best_reduced_cost = reduced_cost;
                best_box = box;
              }
            }
          }
        }
      }
    }

    if (best_reduced_cost < kCostChangeThreshold) {
      if (target) {
        *target = best_box;
      }
      return best_reduced_cost;
    }
    return 0;
  }

  // Add continuous [0,1] box variable with box.Cost() as objective
  // coefficient.  Add to cell constraint of all enclosed cells.
  MPVariable* AddBox(const Box& box) {
    CHECK(boxes_.find(box) == boxes_.end());
    MPVariable* const var = solver_->MakeNumVar(0., 1., box.DebugString());
    solver_->MutableObjective()->SetCoefficient(var, box.Cost());
    max_boxes_constraint_->SetCoefficient(var, 1.0);
    for (int y = box.y_min(); y <= box.y_max(); ++y) {
      for (int x = box.x_min(); x <= box.x_max(); ++x) {
        cell(x, y)->SetCoefficient(var, 1.0);
      }
    }
    boxes_[box] = var;
    return var;
  }

  std::string PrintGrid() const {
    std::string output = StringPrintf("width = %d, height = %d, max_boxes = %d\n",
                                 width_, height_, max_boxes_);
    for (int y = 0; y < height_; ++y) {
      StringAppendF(&output, "%s\n",
                    std::string(grid_ + width_ * y, width_).c_str());
    }
    return output;
  }

  // Prints covering - total cost, those variables with non-zero value,
  // and graphical depiction of covering using upper case letters for
  // integral coverage and lower case for coverage using combination
  // of fractional boxes.
  std::string PrintCovering() const {
    static const double kTolerance = 1e-5;
    std::string output = StringPrintf("cost = %lf\n", solver_->Objective().Value());
    std::unique_ptr<char[]> display(new char[(width_ + 1) * height_ + 1]);
    for (int y = 0; y < height_; ++y) {
      memcpy(display.get() + y * (width_ + 1), grid_ + width_ * y,
             width_);  // Copy the original line.
      display[y * (width_ + 1) + width_] = '\n';
    }
    display[height_ * (width_ + 1)] = '\0';
    int active_box_index = 0;
    for (BoxTable::const_iterator i = boxes_.begin(); i != boxes_.end(); ++i) {
      const double value = i->second->solution_value();
      if (value > kTolerance) {
        const char box_character =
            (i->second->solution_value() >= (1. - kTolerance) ? 'A' : 'a') +
            active_box_index++;
        StringAppendF(&output, "%c: box %s with value %lf\n", box_character,
                      i->first.DebugString().c_str(), value);
        const Box& box = i->first;
        for (int x = box.x_min(); x <= box.x_max(); ++x) {
          for (int y = box.y_min(); y <= box.y_max(); ++y) {
            display[x + y * (width_ + 1)] = box_character;
          }
        }
      }
    }
    output.append(display.get());
    return output;
  }

 protected:
  int index(int x, int y) const { return width_ * y + x; }
  MPConstraint* cell(int x, int y) { return cells_[index(x, y)]; }
  const MPConstraint* cell(int x, int y) const { return cells_[index(x, y)]; }

  // Adds constraints that every cell is covered at most once, exactly
  // once if occupied.
  void AddCellConstraints() {
    cells_.resize(area());
    for (int y = 0; y < height(); ++y) {
      for (int x = 0; x < width(); ++x) {
        cells_[index(x, y)] =
            solver_->MakeRowConstraint(IsCellOccupied(x, y) ? 1. : 0., 1.);
      }
    }
  }

  // Adds constraint on maximum number of boxes used to cover.
  void AddMaxBoxesConstraint() {
    max_boxes_constraint_ = solver_->MakeRowConstraint(0., max_boxes());
  }

  // Gets 2d array element, returning 0 if out-of-bounds.
  double zero_access(const std::vector<double>& array, int x, int y) const {
    if (x < 0 || y < 0) {
      return 0;
    }
    return array[index(x, y)];
  }

  // Precomputes the sum of reduced costs for every upper-left
  // rectangle.
  void ComputeUpperLeftSums(std::vector<double>* upper_left_sums) const {
    for (int y = 0; y < height(); ++y) {
      for (int x = 0; x < width(); ++x) {
        upper_left_sums->operator[](index(x, y)) =
            cell(x, y)->dual_value() + zero_access(*upper_left_sums, x - 1, y) +
            zero_access(*upper_left_sums, x, y - 1) -
            zero_access(*upper_left_sums, x - 1, y - 1);
      }
    }
  }

  typedef std::map<Box, MPVariable*, BoxLessThan> BoxTable;
  MPSolver* const solver_;  // not owned
  const int max_boxes_;
  const int width_;
  const int height_;
  const char* const grid_;
  std::vector<MPConstraint*> cells_;
  BoxTable boxes_;
  MPConstraint* max_boxes_constraint_;
};

// ---------- Main Solve Method ----------

// Solves iteratively using delayed column generation, up to maximum
// number of steps.
void SolveInstance(const Instance& instance,
                   MPSolver::OptimizationProblemType solver_type) {
  // Prepares the solver.
  MPSolver solver("ColumnGeneration", solver_type);
  solver.SuppressOutput();
  solver.MutableObjective()->SetMinimization();

  // Construct problem.
  CoveringProblem problem(&solver, instance);
  CHECK(problem.Init());
  LOG(INFO) << "Initial problem:\n" << problem.PrintGrid();

  int step_number = 0;
  while (step_number < FLAGS_colgen_max_iterations) {
    if (FLAGS_colgen_verbose) {
      LOG(INFO) << "Step number " << step_number;
    }

    // Solve with existing columns.
    CHECK_EQ(MPSolver::OPTIMAL, solver.Solve());
    if (FLAGS_colgen_verbose) {
      LOG(INFO) << problem.PrintCovering();
    }

    // Find optimal new column to add, or stop if none.
    Box box;
    const double reduced_cost = problem.GetOptimalBox(&box);
    if (reduced_cost >= 0) {
      break;
    }

    // Add new column to problem.
    if (FLAGS_colgen_verbose) {
      LOG(INFO) << "Adding " << box.DebugString()
                << ", reduced_cost =" << reduced_cost;
    }
    problem.AddBox(box);

    ++step_number;
  }

  if (step_number >= FLAGS_colgen_max_iterations) {
    // Solve one last time with all generated columns.
    CHECK_EQ(MPSolver::OPTIMAL, solver.Solve());
  }

  LOG(INFO) << step_number << " columns added";
  LOG(INFO) << "Final coverage: " << problem.PrintCovering();
}
}  // namespace operations_research

int main(int argc, char** argv) {
  std::string usage = "column_generation\n";
  usage += "  --colgen_verbose             print verbosely\n";
  usage += "  --colgen_max_iterations <n>  max columns to generate\n";
  usage += "  --colgen_complete            generate all columns at start\n";

  gflags::ParseCommandLineFlags( &argc, &argv, true);

  operations_research::MPSolver::OptimizationProblemType solver_type;
  bool found = false;
  #if defined(USE_GLOP)
  if (FLAGS_colgen_solver == "glop") {
    solver_type = operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING;
    found = true;
  }
  #endif  // USE_GLOP
  #if defined(USE_CLP)
  if (FLAGS_colgen_solver == "clp") {
    solver_type = operations_research::MPSolver::CLP_LINEAR_PROGRAMMING;
    found = true;
  }
  #endif  // USE_CLP
  if (!found) {
    LOG(ERROR) << "Unknown solver " << FLAGS_colgen_solver;
    return 1;
  }

  if (FLAGS_colgen_instance == -1) {
    for (int i = 0; i < operations_research::kInstanceCount; ++i) {
      const operations_research::Instance& instance =
          operations_research::kInstances[i];
      operations_research::SolveInstance(instance, solver_type);
    }
  } else {
    CHECK_GE(FLAGS_colgen_instance, 0);
    CHECK_LT(FLAGS_colgen_instance, operations_research::kInstanceCount);
    const operations_research::Instance& instance =
        operations_research::kInstances[FLAGS_colgen_instance];
    operations_research::SolveInstance(instance, solver_type);
  }
  return 0;
}
