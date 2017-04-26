#include <deque>
#include <vector>
#include <string>
#include <unordered_set>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"

const std::vector<std::vector<int>> small = {
  { 3, 2, -1, 3 },
  { -1, -1, -1, 2 },
  { 3, -1, -1, -1 },
  { 3, -1, 3, 1 }
};

const std::vector<std::vector<int>> medium = {
  { -1, 0, -1, 1, -1, -1, 1, -1 },
  { -1, 3, -1, -1, 2, 3, -1, 2 },
  { -1, -1, 0, -1, -1, -1, -1, 0 },
  { -1, 3, -1, -1, 0, -1, -1, -1 },
  { -1, -1, -1, 3, -1, -1, 0, -1 },
  { 1, -1, -1, -1, -1, 3, -1, -1 },
  { 3, -1, 1, 3, -1, -1, 3, -1 },
  { -1, 0, -1, -1, 3, -1, 3, -1 }
};

const std::vector<std::vector<int>> big = {
  { 3, -1, -1, -1, 2, -1, 1, -1, 1, 2 },
  { 1, -1, 0, -1, 3, -1, 2, 0, -1, -1 },
  { -1, 3, -1, -1, -1, -1, -1, -1, 3, -1 },
  { 2, 0, -1, 3, -1, 2, 3, -1, -1, -1 },
  { -1, -1, -1, 1, 1, 1, -1, -1, 3, 3 },
  { 2, 3, -1, -1, 2, 2, 3, -1, -1, -1 },
  { -1, -1, -1, 1, 2, -1, 2, -1, 3, 3 },
  { -1, 2, -1, -1, -1, -1, -1, -1, 2, -1 },
  { -1, -1, 1, 1, -1, 2, -1, 1, -1, 3 },
  { 3, 3, -1, 1, -1, 2, -1, -1, -1, 2 }
};

namespace operations_research {
namespace {
std::vector<IntVar*> NeighboringArcs(
    int i, int j,
    const std::vector<std::vector<IntVar*>>& h_arcs,
    const std::vector<std::vector<IntVar*>>& v_arcs) {
  std::vector<IntVar*> tmp;
  if (j > 0) {
    tmp.push_back(h_arcs[i][j - 1]);
  }
  if (j < v_arcs.size() - 1) {
    tmp.push_back(h_arcs[i][j]);
  }
  if (i > 0) {
    tmp.push_back(v_arcs[j][i - 1]);
  }
  if (i < h_arcs.size() - 1) {
    tmp.push_back(v_arcs[j][i]);
  }
  return tmp;
}

// Dedicated constraint: Sum(boolvars) is even.
class BooleanSumEven : public Constraint {
 public:
  BooleanSumEven(Solver* const s, const std::vector<IntVar*>& vars)
      : Constraint(s),
        vars_(vars),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {}

  virtual ~BooleanSumEven() {}

  virtual void Post() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumEven::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < vars_.size(); ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    if (num_always_true == num_possible_true && num_possible_true % 2 == 1) {
      solver()->Fail();
    } else if (num_possible_true == num_always_true + 1) {
      DCHECK_NE(-1, possible_true_index);
      vars_[possible_true_index]->SetValue(num_always_true % 2);
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
  }

  void Update(int index) {
    DCHECK(vars_[index]->Bound());
    const int64 value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      num_possible_true_vars_.Decr(solver());
    } else {
      DCHECK_EQ(1, value);
      num_always_true_vars_.Incr(solver());
    }
    if (num_always_true_vars_.Value() == num_possible_true_vars_.Value() &&
        num_possible_true_vars_.Value() % 2 == 1) {
      solver()->Fail();
    } else if (num_possible_true_vars_.Value() ==
               num_always_true_vars_.Value() + 1) {
      int possible_true_index = -1;
      for (int i = 0; i < vars_.size(); ++i) {
        if (!vars_[i]->Bound()) {
          possible_true_index = i;
          break;
        }
      }
      if (possible_true_index != -1) {
        if (num_possible_true_vars_.Value() % 2 == 0) {
          vars_[possible_true_index]->SetMin(1);
        } else {
          vars_[possible_true_index]->SetMax(0);
        }
      }
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("BooleanSumEven([%s])",
                        JoinDebugStringPtr(vars_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  const std::vector<IntVar*> vars_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
};

Constraint* MakeBooleanSumEven(Solver* const s, const std::vector<IntVar*>& v) {
  return s->RevAlloc(new BooleanSumEven(s, v));
}

// Dedicated constraint: There is a single path on the grid.
// This constraint does not enforce the non-crossing, this is done
// by the constraint on the degree of each node.
class GridSinglePath : public Constraint {
 public:
  GridSinglePath(Solver* const solver,
                 const std::vector<std::vector<IntVar*>>& h_arcs,
                 const std::vector<std::vector<IntVar*>>& v_arcs)
      : Constraint(solver),
        h_arcs_(h_arcs),
        v_arcs_(v_arcs) {}

  ~GridSinglePath() {}

  void Post() override {
    Demon* const demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (auto& row : h_arcs_) {
      for (IntVar* const var : row) {
        var->WhenBound(demon);
      }
    }

    for (auto& column : v_arcs_) {
      for (IntVar* const var : column) {
        var->WhenBound(demon);
      }
    }
  }

  // This constraint implements a single propagation.
  // If one point is on the path, it checks the reachability of all possible
  // nodes, and zero out the unreachable parts.
  void InitialPropagate() override {
    const int num_rows = h_arcs_.size();     // number of points
    const int num_columns = v_arcs_.size();  // number of points

    const int num_points = num_rows * num_columns;
    int root_node = -1;
    std::unordered_set<int> possible_points;
    std::vector<std::vector<int>> neighbors(num_points);
    for (int i = 0; i < num_rows; ++i) {
      for (int j = 0; j < num_columns - 1; ++j) {
        IntVar* const h_arc = h_arcs_[i][j];
        if (h_arc->Max() == 1) {
          const int head = i * num_columns + j;
          const int tail = i * num_columns + j + 1;
          neighbors[head].push_back(tail);
          neighbors[tail].push_back(head);
          possible_points.insert(head);
          possible_points.insert(tail);
          if (root_node == -1 && h_arc->Min() == 1) {
            root_node = head;
          }
        }
      }
    }

    for (int i = 0; i < num_rows - 1; ++i) {
      for (int j = 0; j < num_columns; ++j) {
        IntVar* const v_arc = v_arcs_[j][i];
        if (v_arc->Max() == 1) {
          const int head = i * num_columns + j;
          const int tail = (i + 1) * num_columns + j;
          neighbors[head].push_back(tail);
          neighbors[tail].push_back(head);
          possible_points.insert(head);
          possible_points.insert(tail);
          if (root_node == -1 && v_arc->Min() == 1) {
            root_node = head;
          }
        }
      }
    }
    if (root_node == -1) {
      return;
    }
    std::unordered_set<int> visited_points;
    std::deque<int> to_process;
    to_process.push_back(root_node);
    while (!to_process.empty()) {
      const int candidate = to_process.front();
      to_process.pop_front();
      visited_points.insert(candidate);
      for (int neighbor : neighbors[candidate]) {
        if (!ContainsKey(visited_points, neighbor)) {
          to_process.push_back(neighbor);
          visited_points.insert(neighbor);
        }
      }
    }

    if (visited_points.size() < possible_points.size()) {
      for (const int point: visited_points) {
        possible_points.erase(point);
      }
      // Loop on unreachable points and zero all neighboring arcs.
      for (const int point : possible_points) {
        const int i = point / num_columns;
        const int j = point % num_columns;
        const std::vector<IntVar*> neighbors =
            NeighboringArcs(i, j, h_arcs_, v_arcs_);
        for (IntVar* const var : neighbors) {
          var->SetMax(0);
        }
      }
    }
  }

 private:
  const std::vector<std::vector<IntVar*>> h_arcs_;
  const std::vector<std::vector<IntVar*>> v_arcs_;
};

Constraint* MakeSingleLoop(Solver* const solver,
                           const std::vector<std::vector<IntVar*>>& h_arcs,
                           const std::vector<std::vector<IntVar*>>& v_arcs) {
  return solver->RevAlloc(new GridSinglePath(solver, h_arcs, v_arcs));
}

void PrintSolution(const std::vector<std::vector<int>>& data,
                   const std::vector<std::vector<IntVar*>>& h_arcs,
                   const std::vector<std::vector<IntVar*>>& v_arcs) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  for (int i = 0; i < num_rows; ++i) {
    std::string first_line;
    std::string second_line;
    std::string third_line;
    for (int j = 0; j < num_columns; ++j) {
      const int h_arc = h_arcs[i][j]->Value();
      const int v_arc = v_arcs[j][i]->Value();
      const int sum = data[i][j];
      first_line += h_arc == 1 ? " ---" : "    ";
      second_line += v_arc == 1 ? "|" : " ";
      second_line += sum == -1 ? "   " : StringPrintf(" %d ", sum).c_str();
      third_line += v_arc == 1 ? "|   " : "    ";
    }
    const int termination = v_arcs[num_columns][i]->Value();
    second_line += termination == 1 ? "|" : " ";
    third_line += termination == 1 ? "|" : " ";
    std::cout << first_line << std::endl;
    std::cout << third_line << std::endl;
    std::cout << second_line << std::endl;
    std::cout << third_line << std::endl;
  }
  std::string last_line;
  for (int j = 0; j < num_columns; ++j) {
    const int h_arc = h_arcs[num_rows][j]->Value();
    last_line += h_arc == 1 ? " ---" : "    ";
  }
  std::cout << last_line << std::endl;
}
}  // namespace

void SlitherLink(const std::vector<std::vector<int>>& data) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  Solver solver("slitherlink");
  std::vector<IntVar*> all_vars;
  std::vector<std::vector<IntVar*>> h_arcs(num_rows + 1);
  for (int i = 0; i < num_rows + 1; ++i) {
    solver.MakeBoolVarArray(num_columns, StringPrintf("h_arc_%i_", i),
                            &h_arcs[i]);
    all_vars.insert(all_vars.end(), h_arcs[i].begin(), h_arcs[i].end());
  }

  std::vector<std::vector<IntVar*>> v_arcs(num_columns + 1);
  for (int i = 0; i < num_columns + 1; ++i) {
    solver.MakeBoolVarArray(num_rows, StringPrintf("v_arc_%i_", i), &v_arcs[i]);
    all_vars.insert(all_vars.end(), v_arcs[i].begin(), v_arcs[i].end());
  }

  // Constraint on the sum of arcs.
  for (int i = 0; i < num_rows; ++i) {
    for (int j = 0; j < num_columns; ++j) {
      const int value = data[i][j];
      if (value != -1) {
        std::vector<IntVar*> square = { h_arcs[i][j], h_arcs[i + 1][j],
                                         v_arcs[j][i], v_arcs[j + 1][i] };
        solver.AddConstraint(solver.MakeSumEquality(square, value));
      }
    }
  }

  // Single loop: each node has a degree 0 or 2.
  const std::vector<int> zero_or_two = { 0, 2 };
  for (int i = 0; i < num_rows + 1; ++i) {
    for (int j = 0; j < num_columns + 1; ++j) {
      const std::vector<IntVar*> neighbors =
          NeighboringArcs(i, j, h_arcs, v_arcs);
      solver.AddConstraint(
          solver.MakeMemberCt(solver.MakeSum(neighbors), zero_or_two));
    }
  }

  // Single loop: sum of arc on row or column is even
  for (int i = 0; i < num_columns; ++i) {
    std::vector<IntVar*> column;
    for (int j = 0; j < num_rows + 1; ++j) {
      column.push_back(h_arcs[j][i]);
    }
    solver.AddConstraint(MakeBooleanSumEven(&solver, column));
  }
  for (int i = 0; i < num_rows; ++i) {
    std::vector<IntVar*> row;
    for (int j = 0; j < num_columns + 1; ++j) {
      row.push_back(v_arcs[j][i]);
    }
    solver.AddConstraint(MakeBooleanSumEven(&solver, row));
  }

  // Hamiltonian path: add single path constraint.
  solver.AddConstraint(MakeSingleLoop(&solver, h_arcs, v_arcs));

  // Special rule on corners: value == 3 implies 2 border arcs used.
  if (data[0][0] == 3) {
    h_arcs[0][0]->SetMin(1);
    v_arcs[0][0]->SetMin(1);
  }
  if (data[0][num_columns - 1] == 3) {
    h_arcs[0][num_columns - 1]->SetMin(1);
    v_arcs[num_columns][0]->SetMin(1);
  }
  if (data[num_rows - 1][0] == 3) {
    h_arcs[num_rows][0]->SetMin(1);
    v_arcs[0][num_rows - 1]->SetMin(1);
  }
  if (data[num_rows - 1][num_columns - 1] == 3) {
    h_arcs[num_rows][num_columns - 1]->SetMin(1);
    v_arcs[num_columns][num_rows - 1]->SetMin(1);
  }

  // Search.
  DecisionBuilder* const db = solver.MakePhase(
      all_vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE);

  SearchMonitor* const log = solver.MakeSearchLog(1000000);

  solver.NewSearch(db, log);
  while (solver.NextSolution()) {
    PrintSolution(data, h_arcs, v_arcs);
  }
  solver.EndSearch();
}
} // namespace operations_research

int main() {
  std::cout << "Small problem" << std::endl;
  operations_research::SlitherLink(small);
  std::cout << "Medium problem" << std::endl;
  operations_research::SlitherLink(medium);
  std::cout << "Big problem" << std::endl;
  operations_research::SlitherLink(big);
  return 0;
}
