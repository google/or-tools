#include <cstddef>
#include <iostream>
#include <vector>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

int main() {
  using ::operations_research::Domain;
  using ::operations_research::sat::BoolVar;
  using ::operations_research::sat::CpModelBuilder;
  using ::operations_research::sat::CpSolverStatus;
  using ::operations_research::sat::LinearExpr;
  using ::operations_research::sat::Model;
  using ::operations_research::sat::SatParameters;
  using ::operations_research::sat::SolveCpModel;

  std::vector<int> w = {3, 4, 5, 5};
  std::vector<int> c = {9, 9};

  CpModelBuilder builder;
  std::vector<std::vector<BoolVar>> assignment(w.size());
  for (size_t i = 0; i < w.size(); i++) {
    assignment[i].reserve(c.size());
    for (size_t j = 0; j < c.size(); j++) {
      assignment[i].emplace_back(builder.NewBoolVar());
    }
    builder.AddExactlyOne(assignment[i]);
  }
  for (size_t j = 0; j < c.size(); j++) {
    LinearExpr expr;
    for (size_t i = 0; i < w.size(); i++) {
      expr += LinearExpr::Term(assignment[i][j], w[i]);
    }
    builder.AddLinearConstraint(expr, Domain(0, c[j]));
  }
  Model model;
  SatParameters parameters;
  parameters.set_num_search_workers(4);
  parameters.set_max_time_in_seconds(10.0);
  model.Add(NewSatParameters(parameters));
  auto response = SolveCpModel(builder.Build(), &model);
  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    std::cout << "all ok";
  }
}
