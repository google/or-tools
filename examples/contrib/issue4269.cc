#include <iostream>
#include <vector>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_checker.h"

using namespace operations_research;
using namespace operations_research::sat;

int main() {
    std::vector<int> w = {3, 4, 5, 5};
    std::vector<int> c = {9, 9};

    sat::CpModelBuilder builder;
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
    sat::Model model;
    sat::SatParameters parameters;
    parameters.set_num_search_workers(4);
    parameters.set_max_time_in_seconds(10.0);
    model.Add(sat::NewSatParameters(parameters));
    auto response = sat::SolveCpModel(builder.Build(), &model);
    if (response.status() == sat::CpSolverStatus::OPTIMAL || response.status() == sat::CpSolverStatus::FEASIBLE) {
        std::cout << "all ok";
    }
}
