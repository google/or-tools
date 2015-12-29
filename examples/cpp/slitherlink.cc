#include <vector>
#include <string>

#include "constraint_solver/constraint_solver.h"

namespace operations_research {
const std::vector<std::vector<int>> data = { {  3,  2, -1,  3 },
                                             { -1, -1, -1,  2 },
                                             {  3, -1, -1, -1 },
                                             {  3, -1,  3,  1 } };

const std::vector<std::vector<int>> data2 = { { -1, 0, -1, 1, -1, -1, 1, -1 },
                                              { -1, 3, -1, -1, 2, 3, -1, 2 },
                                              { -1, -1, 0, -1, -1, -1, -1, 0 },
                                              { -1, 3, -1, -1, 0, -1, -1, -1 },
                                              { -1, -1, -1, 3, -1, -1, 0, -1 },
                                              { 1, -1, -1, -1, -1, 3, -1, -1 },
                                              { 3, -1, 1, 3, -1, -1, 3, -1 },
                                              { -1, 0, -1, -1, 3, -1, 3, -1 } };

const std::vector<int> zero_or_two = { 0, 2 };

void PrintSolution(const std::vector<std::vector<int>>& data,
                   const std::vector<std::vector<IntVar*>>& h_arcs,
                   const std::vector<std::vector<IntVar*>>& v_arcs) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  for (int i = 0; i < num_rows; ++i) {
    std::string first_line;
    std::string second_line;
    for (int j = 0; j < num_columns; ++j) {
      const int h_arc = h_arcs[i][j]->Value();
      const int v_arc = v_arcs[j][i]->Value();
      const int sum = data[i][j];
      first_line +=  h_arc == 1 ? " -" : "  ";
      second_line += v_arc == 1 ? "|" : " ";
      second_line += sum == -1 ? " " : StringPrintf("%d", sum).c_str();
    }
    const int termination = v_arcs[num_columns][i]->Value();
    second_line += termination == 1 ? "|" : " ";
    std::cout << first_line << std::endl;
    std::cout << second_line << std::endl;
  }
  std::string last_line;
  for (int j = 0; j < num_columns; ++j) {
    const int h_arc = h_arcs[num_rows][j]->Value();
    last_line +=  h_arc == 1 ? " -" : "  ";
  }
  std::cout << last_line << std::endl;
}

void Solve(const std::vector<std::vector<int>>& data) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  Solver solver("slitherlink");
  std::vector<IntVar*> all_vars;
  std::vector<std::vector<IntVar*>> h_arcs(num_rows + 1);
  for (int i = 0; i < num_rows + 1; ++i) {
    solver.MakeBoolVarArray(
        num_columns, StringPrintf("h_arc_%i_", i), &h_arcs[i]);
    all_vars.insert(all_vars.end(), h_arcs[i].begin(), h_arcs[i].end());
  }

  std::vector<std::vector<IntVar*>> v_arcs(num_columns + 1);
  for (int i = 0; i < num_columns + 1; ++i) {
    solver.MakeBoolVarArray(
        num_rows, StringPrintf("v_arc_%i_", i), &v_arcs[i]);
    all_vars.insert(all_vars.end(), v_arcs[i].begin(), v_arcs[i].end());
  }

  // Constraint on the sum of arcs.
  for (int i = 0; i < num_rows; ++i) {
    for (int j = 0; j < num_columns; ++j) {
      const int value = data[i][j];
      if (value != -1) {
        std::vector<IntVar*> square = { h_arcs[i][j],
                                        h_arcs[i + 1][j],
                                        v_arcs[j][i],
                                        v_arcs[j + 1][i] };
        solver.AddConstraint(solver.MakeSumEquality(square, value));
      }
    }
  }

  // Hamiltonian path: each node has a degree 0 or 2.
  for (int i = 0; i < num_rows + 1; ++i) {
    for (int j = 0; j < num_columns + 1; ++j) {
      std::vector<IntVar*> tmp;
      if (j > 0) {
        tmp.push_back(h_arcs[i][j - 1]);
      }
      if (j < num_columns) {
        tmp.push_back(h_arcs[i][j]);
      }
      if (i > 0) {
        tmp.push_back(v_arcs[j][i - 1]);
      }
      if (i < num_rows) {
        tmp.push_back(v_arcs[j][i]);
      }
      solver.AddConstraint(
          solver.MakeMemberCt(solver.MakeSum(tmp), zero_or_two));
    }
  }

  // Hamiltonian path: add single path constraint.

  // Search.
  DecisionBuilder* const db =
      solver.MakePhase(all_vars,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE);

  SearchMonitor* const log = solver.MakeSearchLog(1000000);

  solver.NewSearch(db, log);
  while (solver.NextSolution()) {
    PrintSolution(data, h_arcs, v_arcs);
  }
  solver.EndSearch();
}
}  // namespace operations_research

int main() {
  operations_research::Solve(operations_research::data2);
  return 0;
}
