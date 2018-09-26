#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void Solve() {
  CpModelProto cp_model;
  cp_model.set_name("variable intervals");

  // Helpers. (will be obsolete in the next release (v6.10))
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&cp_model, &new_variable](int64 v) {
    return new_variable(v, v);
  };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->mutable_interval()->set_start(start);
    ct->mutable_interval()->set_size(duration);
    ct->mutable_interval()->set_end(end);
    return index;
  };

  auto add_precedence = [&cp_model](int before, int after) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(0);
    lin->add_domain(kint64max);
  };

  auto add_sum_equal = [&cp_model](const std::vector<int> vars, int64 value) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    for (const int var : vars) {
      lin->add_vars(var);
      lin->add_coeffs(1L);
    }
    lin->add_domain(value);
    lin->add_domain(value);
  };

  auto add_no_overlap = [&cp_model](const std::vector<int>& intervals) {
    NoOverlapConstraintProto* const no_overlap =
        cp_model.add_constraints()->mutable_no_overlap();
    for (const int i : intervals) {
      no_overlap->add_intervals(i);
    }
  };

  // Actual model.

  const int start_ins = new_variable(660, 755);
  const int duration_ins = new_constant(25);
  const int end_ins = new_variable(685, 780);
  const int ins = new_interval(start_ins, duration_ins, end_ins);

  const int start_p1 = new_variable(500, 800);
  const int duration_p1 = new_variable(1, 360);
  const int end_p1 = new_variable(500, 1000);
  const int p1 = new_interval(start_p1, duration_p1, end_p1);

  const int start_p2 = new_variable(500, 800);
  const int duration_p2 = new_variable(1, 360);
  const int end_p2 = new_variable(500, 1000);
  const int p2 = new_interval(start_p2, duration_p2, end_p2);

  add_sum_equal({duration_p1, duration_p2}, 360);
  add_precedence(end_p1, start_p2);

  add_no_overlap({ins, p1, p2});

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  start_p1 = " << r.solution(start_p1);
    LOG(INFO) << "  duration_p1 = " << r.solution(duration_p1);
    LOG(INFO) << "  start_p2 = " << r.solution(start_p2);
    LOG(INFO) << "  duration_p2 = " << r.solution(duration_p2);
    LOG(INFO) << "  start_ins = " << r.solution(start_ins);
    num_solutions++;
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::Solve();

  return EXIT_SUCCESS;
}
