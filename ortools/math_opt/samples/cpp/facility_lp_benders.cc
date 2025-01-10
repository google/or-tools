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

// An advanced benders decomposition example
//
// We consider a network design problem where each location has a demand that
// must be met by its neighboring facilities, and each facility can control
// its total capacity. In this version we also require that locations cannot
// use more that a specified fraction of a facilities capacity.
//
// Problem data:
// * F: set of facilities.
// * L: set of locations.
// * E: subset of {(f,l) : f in F, l in L} that describes the network between
//      facilities and locations.
// * d: demand at location (all demands are equal for simplicity).
// * c: cost per unit of capacity at a facility (all facilities are have the
//      same cost for simplicity).
// * h: cost per unit transported through an edge.
// * a: fraction of a facility's capacity that can be used by each location.
//
// Decision variables:
// * z_f: capacity at facility f in F.
// * x_(f,l): flow from facility f to location l for all (f,l) in E.
//
// Formulation:
//
//   min c * sum(z_f : f in F) + sum(h_e * x_e : e in E)
//   s.t.
//                                   x_(f,l) <= a * z_f   for all (f,l) in E
//     sum(x_(f,l) : l such that (f,l) in E) <=     z_f   for all f in F
//     sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
//                                       x_e >= 0     for all e in E
//                                       z_f >= 0     for all f in F
//
// Below we solve this problem directly and using a benders decompostion
// approach.

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/random/seed_sequences.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/status_macros.h"

ABSL_FLAG(int, num_facilities, 750, "Number of facilities.");
ABSL_FLAG(int, num_locations, 12, "Number of locations.");
ABSL_FLAG(double, edge_probability, 0.99, "Edge probability.");
ABSL_FLAG(double, benders_precission, 1e-9, "Benders target precission.");
ABSL_FLAG(double, location_demand, 1, "Client demands.");
ABSL_FLAG(double, facility_cost, 100, "Facility capacity cost.");
ABSL_FLAG(
    double, location_fraction, 0.001,
    "Fraction of a facility's capacity that can be used by each location.");
ABSL_FLAG(operations_research::math_opt::SolverType, solver_type,
          operations_research::math_opt::SolverType::kGlop,
          "the LP solver to use, possible values: glop, gurobi, glpk, pdlp");

namespace {

namespace math_opt = operations_research::math_opt;
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kZeroTol = 1.0e-3;

////////////////////////////////////////////////////////////////////////////////
// Facility location instance representation and generation
////////////////////////////////////////////////////////////////////////////////

// First element is a facility and second is a location.
using Edge = std::pair<int, int>;

// A simple randomly-generated facility-location network.
class Network {
 public:
  Network(int num_facilities, int num_locations, double edge_probability);

  int num_facilities() const { return num_facilities_; }
  int num_locations() const { return num_locations_; }

  const std::vector<Edge>& edges() const { return edges_; }

  const std::vector<Edge>& edges_incident_to_facility(
      const int facility) const {
    return facility_edge_incidence_[facility];
  }

  const std::vector<Edge>& edges_incident_to_location(
      const int location) const {
    return location_edge_incidence_[location];
  }

  double edge_cost(const Edge& edge) const { return edge_costs_.at(edge); }

 private:
  int num_facilities_;
  int num_locations_;
  // No order is assumed for the following lists of edges.
  std::vector<Edge> edges_;
  absl::flat_hash_map<Edge, double> edge_costs_;
  std::vector<std::vector<Edge>> facility_edge_incidence_;
  std::vector<std::vector<Edge>> location_edge_incidence_;
};

Network::Network(const int num_facilities, const int num_locations,
                 const double edge_probability) {
  absl::SeedSeq seq({1, 2, 3});
  absl::BitGen bitgen(seq);

  num_facilities_ = num_facilities;
  num_locations_ = num_locations;
  facility_edge_incidence_ = std::vector<std::vector<Edge>>(num_facilities);
  location_edge_incidence_ =
      std::vector<std::vector<Edge>>(num_locations, std::vector<Edge>());
  for (int facility = 0; facility < num_facilities_; ++facility) {
    for (int location = 0; location < num_locations_; ++location) {
      if (absl::Bernoulli(bitgen, edge_probability)) {
        const Edge edge({facility, location});
        facility_edge_incidence_[facility].push_back(edge);
        location_edge_incidence_[location].push_back(edge);
        edges_.push_back(edge);
        edge_costs_.insert({edge, absl::Uniform(bitgen, 0, 1.0)});
      }
    }
  }
  // Ensure every facility is connected to at least one location and every
  // location is connected to at least one facility.
  for (int facility = 0; facility < num_facilities; ++facility) {
    auto& locations = facility_edge_incidence_[facility];
    if (locations.empty()) {
      const int location =
          absl::uniform_int_distribution<int>(0, num_locations - 1)(bitgen);
      const std::pair<int, int> edge({facility, location});
      locations.push_back(edge);
      location_edge_incidence_[location].push_back(edge);
      edges_.push_back(edge);
      edge_costs_.insert({edge, absl::Uniform(bitgen, 0, 1.0)});
    }
  }
  for (int location = 0; location < num_locations; ++location) {
    auto& facilities = location_edge_incidence_[location];
    if (facilities.empty()) {
      const int facility =
          absl::uniform_int_distribution<int>(0, num_facilities - 1)(bitgen);
      const std::pair<int, int> edge({facility, location});
      facilities.push_back(edge);
      facility_edge_incidence_[facility].push_back(edge);
      edges_.push_back(edge);
      edge_costs_.insert({edge, absl::Uniform(bitgen, 0, 1.0)});
    }
  }
}

struct FacilityLocationInstance {
  Network network;
  double location_demand;
  double facility_cost;
  double location_fraction;
};

////////////////////////////////////////////////////////////////////////////////
// Direct solve
////////////////////////////////////////////////////////////////////////////////

// See file level comment for problem description and formulation.
absl::Status FullProblem(const FacilityLocationInstance& instance,
                         const math_opt::SolverType solver_type) {
  const int num_facilities = instance.network.num_facilities();
  const int num_locations = instance.network.num_locations();

  math_opt::Model model("Full network design problem");

  // Capacity variables
  std::vector<math_opt::Variable> z;
  for (int j = 0; j < num_facilities; j++) {
    z.push_back(model.AddContinuousVariable(0.0, kInf));
  }

  // Flow variables
  absl::flat_hash_map<Edge, math_opt::Variable> x;
  for (const auto& edge : instance.network.edges()) {
    const math_opt::Variable x_edge = model.AddContinuousVariable(0.0, kInf);
    x.insert({edge, x_edge});
  }

  // Objective Function
  math_opt::LinearExpression objective_for_edges;
  for (const auto& [edge, x_edge] : x) {
    objective_for_edges += instance.network.edge_cost(edge) * x_edge;
  }
  model.Minimize(objective_for_edges +
                 instance.facility_cost * math_opt::Sum(z));

  // Demand constraints
  for (int location = 0; location < num_locations; ++location) {
    math_opt::LinearExpression incoming_supply;
    for (const auto& edge :
         instance.network.edges_incident_to_location(location)) {
      incoming_supply += x.at(edge);
    }
    model.AddLinearConstraint(incoming_supply >= instance.location_demand);
  }

  // Supply constraints
  for (int facility = 0; facility < num_facilities; ++facility) {
    math_opt::LinearExpression outgoing_supply;
    for (const auto& edge :
         instance.network.edges_incident_to_facility(facility)) {
      outgoing_supply += x.at(edge);
    }
    model.AddLinearConstraint(outgoing_supply <= z[facility]);
  }

  // Arc constraints
  for (int facility = 0; facility < num_facilities; ++facility) {
    for (const auto& edge :
         instance.network.edges_incident_to_facility(facility)) {
      model.AddLinearConstraint(x.at(edge) <=
                                instance.location_fraction * z[facility]);
    }
  }
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   math_opt::Solve(model, solver_type));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());

  std::cout << "Full problem optimal objective: "
            << absl::StrFormat("%.9f", result.objective_value()) << std::endl;
  return absl::OkStatus();
}

////////////////////////////////////////////////////////////////////////////////
// Benders solver
////////////////////////////////////////////////////////////////////////////////

// Setup first stage model:
//
//   min c * sum(z_f : f in F) + w
//   s.t.
//                                       z_f >= 0     for all f in F
//          sum(fcut_f^i z_f) + fcut_const^i <= 0      for i = 1,...
//          sum(ocut_f^j z_f) + ocut_const^j <= w      for j = 1,...
struct FirstStageProblem {
  math_opt::Model model;
  std::vector<math_opt::Variable> z;
  math_opt::Variable w;

  FirstStageProblem(const Network& network, double facility_cost);
};

FirstStageProblem::FirstStageProblem(const Network& network,
                                     const double facility_cost)
    : model("First stage problem"), w(model.AddContinuousVariable(0.0, kInf)) {
  const int num_facilities = network.num_facilities();

  // Capacity variables
  for (int j = 0; j < num_facilities; j++) {
    z.push_back(model.AddContinuousVariable(0.0, kInf));
  }

  // First stage objective
  model.Minimize(w + facility_cost * Sum(z));
}

// Represents a cut if the form:
//
//   z_coefficients^T z + constant <= w_coefficient * w
//
// This will be a feasibility cut if w_coefficient = 0.0 and an optimality
// cut if w_coefficient = 1.
struct Cut {
  std::vector<double> z_coefficients;
  double constant;
  double w_coefficient;
};

// Solves the second stage model:
//
//   min sum(h_e * x_e : e in E)
//   s.t.
//                                   x_(f,l) <= a * zz_f   for all (f,l) in E
//     sum(x_(f,l) : l such that (f,l) in E) <=     zz_f   for all f in F
//     sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
//                                       x_e >= 0     for all e in E
//
// where zz_f are fixed values for z_f from the first stage model, and generates
// an infeasibility or optimality cut as needed.
class SecondStageSolver {
 public:
  static absl::StatusOr<std::unique_ptr<SecondStageSolver>> New(
      FacilityLocationInstance instance, math_opt::SolverType solver_type);

  absl::StatusOr<std::pair<double, Cut>> Solve(
      absl::Span<const double> z_values, double w_value,
      double fist_stage_objective);

 private:
  SecondStageSolver(FacilityLocationInstance instance,
                    math_opt::SolveParameters second_stage_params);

  absl::StatusOr<Cut> OptimalityCut(
      const math_opt::SolveResult& second_stage_result);
  absl::StatusOr<Cut> FeasibilityCut(
      const math_opt::SolveResult& second_stage_result);

  math_opt::Model second_stage_model_;
  const Network network_;
  const double location_fraction_;
  math_opt::SolveParameters second_stage_params_;

  absl::flat_hash_map<Edge, math_opt::Variable> x_;
  std::vector<math_opt::LinearConstraint> supply_constraints_;
  std::vector<math_opt::LinearConstraint> demand_constraints_;
  std::unique_ptr<math_opt::IncrementalSolver> solver_;
};

absl::StatusOr<math_opt::SolveParameters> EnsureDualRaySolveParameters(
    const math_opt::SolverType solver_type) {
  math_opt::SolveParameters parameters;
  switch (solver_type) {
    case math_opt::SolverType::kGurobi:
      parameters.gurobi.param_values["InfUnbdInfo"] = "1";
      break;
    case math_opt::SolverType::kGlop:
      parameters.presolve = math_opt::Emphasis::kOff;
      parameters.scaling = math_opt::Emphasis::kOff;
      parameters.lp_algorithm = math_opt::LPAlgorithm::kDualSimplex;
      break;
    case math_opt::SolverType::kGlpk:
      parameters.presolve = math_opt::Emphasis::kOff;
      parameters.lp_algorithm = math_opt::LPAlgorithm::kDualSimplex;
      parameters.glpk.compute_unbound_rays_if_possible = true;
      break;
    default:
      return util::InternalErrorBuilder()
             << "unsupported solver: " << solver_type;
  }
  return parameters;
}

absl::StatusOr<std::unique_ptr<SecondStageSolver>> SecondStageSolver::New(
    FacilityLocationInstance instance, const math_opt::SolverType solver_type) {
  // Set solver arguments to ensure a dual ray is returned.
  ASSIGN_OR_RETURN(math_opt::SolveParameters parameters,
                   EnsureDualRaySolveParameters(solver_type));

  std::unique_ptr<SecondStageSolver> second_stage_solver =
      absl::WrapUnique<SecondStageSolver>(
          new SecondStageSolver(std::move(instance), parameters));
  ASSIGN_OR_RETURN(std::unique_ptr<math_opt::IncrementalSolver> solver,
                   math_opt::NewIncrementalSolver(
                       &second_stage_solver->second_stage_model_, solver_type));
  second_stage_solver->solver_ = std::move(solver);
  return std::move(second_stage_solver);
}

SecondStageSolver::SecondStageSolver(
    FacilityLocationInstance instance,
    math_opt::SolveParameters second_stage_params)
    : second_stage_model_("Second stage model"),
      network_(std::move(instance.network)),
      location_fraction_(instance.location_fraction),
      second_stage_params_(second_stage_params) {
  const int num_facilities = network_.num_facilities();
  const int num_locations = network_.num_locations();

  // Flow variables
  for (const auto& edge : network_.edges()) {
    const math_opt::Variable x_edge =
        second_stage_model_.AddContinuousVariable(0.0, kInf);
    x_.insert({edge, x_edge});
  }

  // Objective Function
  math_opt::LinearExpression objective_for_edges;
  for (const auto& [edge, x_edge] : x_) {
    objective_for_edges += network_.edge_cost(edge) * x_edge;
  }
  second_stage_model_.Minimize(objective_for_edges);

  // Demand constraints
  for (int location = 0; location < num_locations; ++location) {
    math_opt::LinearExpression incoming_supply;
    for (const auto& edge : network_.edges_incident_to_location(location)) {
      incoming_supply += x_.at(edge);
    }
    demand_constraints_.push_back(second_stage_model_.AddLinearConstraint(
        incoming_supply >= instance.location_demand));
  }

  // Supply constraints
  for (int facility = 0; facility < num_facilities; ++facility) {
    math_opt::LinearExpression outgoing_supply;
    for (const auto& edge : network_.edges_incident_to_facility(facility)) {
      outgoing_supply += x_.at(edge);
    }
    // Set supply constraint with trivial upper bound to be updated with first
    // stage information.
    supply_constraints_.push_back(
        second_stage_model_.AddLinearConstraint(outgoing_supply <= kInf));
  }
}

absl::StatusOr<std::pair<double, Cut>> SecondStageSolver::Solve(
    absl::Span<const double> z_values, const double w_value,
    const double fist_stage_objective) {
  const int num_facilities = network_.num_facilities();

  // Update second stage with first stage solution.
  for (int facility = 0; facility < num_facilities; ++facility) {
    if (z_values[facility] < -kZeroTol) {
      return util::InternalErrorBuilder()
             << "negative z_value in first stage: " << z_values[facility]
             << " for facility " << facility;
    }
    // Make sure variable bounds are valid (lb <= ub).
    const double capacity_value = std::max(z_values[facility], 0.0);
    for (const auto& edge : network_.edges_incident_to_facility(facility)) {
      second_stage_model_.set_upper_bound(x_.at(edge),
                                          location_fraction_ * capacity_value);
    }
    second_stage_model_.set_upper_bound(supply_constraints_[facility],
                                        capacity_value);
  }
  // Solve and process second stage.
  ASSIGN_OR_RETURN(const math_opt::SolveResult second_stage_result,
                   solver_->Solve(math_opt::SolveArguments{
                       .parameters = second_stage_params_}));
  switch (second_stage_result.termination.reason) {
    case math_opt::TerminationReason::kInfeasible:
      // If the second stage problem is infeasible we can construct a
      // feasibility cut from a returned dual ray.
      {
        OR_ASSIGN_OR_RETURN3(const Cut feasibility_cut,
                             FeasibilityCut(second_stage_result),
                             _ << "on infeasible for second stage solver");
        return std::make_pair(kInf, feasibility_cut);
      }
    case math_opt::TerminationReason::kOptimal:
      // If the second stage problem is optimal we can construct an optimality
      // cut from a returned dual optimal solution. We can also update the upper
      // bound.
      {
        // Upper bound is obtained by switching predicted second stage objective
        // value w with the true second stage objective value.
        const double upper_bound = fist_stage_objective - w_value +
                                   second_stage_result.objective_value();
        OR_ASSIGN_OR_RETURN3(const Cut optimality_cut,
                             OptimalityCut(second_stage_result),
                             _ << "on optimal for second stage solver");
        return std::make_pair(upper_bound, optimality_cut);
      }
    default:
      return util::InternalErrorBuilder()
             << "second stage was not solved to optimality or infeasibility: "
             << second_stage_result.termination;
  }
}

// If the second stage problem is infeasible we get a dual ray (r, y) such that
//
// sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
// + sum(y_f*zz_f : f in F, y_f < 0)
// + sum(y_l*d : l in L, y_l > 0) > 0.
//
// Then we get the feasibility cut.
//
// sum(fcut_f*z_f) + fcut_const <= 0,
//
// where
//
// fcut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
//              + min{y_f, 0}
// fcut_const = sum*(y_l*d : l in L, y_l > 0)
absl::StatusOr<Cut> SecondStageSolver::FeasibilityCut(
    const math_opt::SolveResult& second_stage_result) {
  const int num_facilities = network_.num_facilities();
  Cut result;

  if (!second_stage_result.has_dual_ray()) {
    // MathOpt does not require solvers to return a dual ray on infeasible,
    // but most LP solvers always will.
    return util::InternalErrorBuilder()
           << "no dual ray available for feasibility cut";
  }

  for (int facility = 0; facility < num_facilities; ++facility) {
    double coefficient = 0.0;
    for (const auto& edge : network_.edges_incident_to_facility(facility)) {
      const double reduced_cost =
          second_stage_result.ray_reduced_costs().at(x_.at(edge));
      coefficient += location_fraction_ * std::min(reduced_cost, 0.0);
    }
    const double dual_value =
        second_stage_result.ray_dual_values().at(supply_constraints_[facility]);
    coefficient += std::min(dual_value, 0.0);
    result.z_coefficients.push_back(coefficient);
  }
  result.constant = 0.0;
  for (const auto& constraint : demand_constraints_) {
    const double dual_value =
        second_stage_result.ray_dual_values().at(constraint);
    result.constant += std::max(dual_value, 0.0);
  }
  result.w_coefficient = 0.0;
  return result;
}

// If the second stage problem is optimal we get a dual solution (r, y) such
// that the optimal objective value is equal to
//
// sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
// + sum(y_f*zz_f : f in F, y_f < 0)
// + sum*(y_l*d : l in L, y_l > 0) > 0.
//
// Then we get the optimality cut.
//
// sum(ocut_f*z_f) + ocut_const <= w,
//
// where
//
// ocut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
//              + min{y_f, 0}
// ocut_const = sum*(y_l*d : l in L, y_l > 0)
absl::StatusOr<Cut> SecondStageSolver::OptimalityCut(
    const math_opt::SolveResult& second_stage_result) {
  const int num_facilities = network_.num_facilities();
  Cut result;

  if (!second_stage_result.has_dual_feasible_solution()) {
    // MathOpt does not require solvers to return a dual solution on optimal,
    // but most LP solvers always will.
    return util::InternalErrorBuilder()
           << "no dual solution available for optimality cut";
  }

  for (int facility = 0; facility < num_facilities; ++facility) {
    double coefficient = 0.0;
    for (const auto& edge : network_.edges_incident_to_facility(facility)) {
      const double reduced_cost =
          second_stage_result.reduced_costs().at(x_.at(edge));
      coefficient += location_fraction_ * std::min(reduced_cost, 0.0);
    }
    double dual_value =
        second_stage_result.dual_values().at(supply_constraints_[facility]);
    coefficient += std::min(dual_value, 0.0);
    result.z_coefficients.push_back(coefficient);
  }
  result.constant = 0.0;
  for (const auto& constraint : demand_constraints_) {
    double dual_value = second_stage_result.dual_values().at(constraint);
    result.constant += std::max(dual_value, 0.0);
  }
  result.w_coefficient = 1.0;
  return result;
}

absl::Status Benders(const FacilityLocationInstance& instance,
                     const double target_precission,
                     const math_opt::SolverType solver_type,
                     const int maximum_iterations = 30000) {
  const int num_facilities = instance.network.num_facilities();

  // Setup first stage model and solver.
  FirstStageProblem first_stage(instance.network, instance.facility_cost);
  ASSIGN_OR_RETURN(
      const std::unique_ptr<math_opt::IncrementalSolver> first_stage_solver,
      math_opt::NewIncrementalSolver(&first_stage.model, solver_type));
  // Setup second stage solver.
  ASSIGN_OR_RETURN(std::unique_ptr<SecondStageSolver> second_stage_solver,
                   SecondStageSolver::New(instance, solver_type));
  // Start Benders
  int iteration = 0;
  double best_upper_bound = kInf;
  std::vector<double> z_values;
  z_values.resize(num_facilities);
  while (true) {
    LOG(INFO) << "Iteration: " << iteration;

    // Solve and process first stage.
    ASSIGN_OR_RETURN(const math_opt::SolveResult first_stage_result,
                     first_stage_solver->Solve());
    RETURN_IF_ERROR(first_stage_result.termination.EnsureIsOptimal())
        << " in first stage problem";
    for (int j = 0; j < num_facilities; j++) {
      z_values[j] = first_stage_result.variable_values().at(first_stage.z[j]);
    }
    const double lower_bound = first_stage_result.objective_value();
    LOG(INFO) << "LB = " << lower_bound;
    // Solve and process second stage.
    ASSIGN_OR_RETURN(
        (auto [upper_bound, cut]),
        second_stage_solver->Solve(
            z_values, first_stage_result.variable_values().at(first_stage.w),
            first_stage_result.objective_value()));
    math_opt::LinearExpression cut_expression;
    for (int j = 0; j < num_facilities; j++) {
      cut_expression += cut.z_coefficients[j] * first_stage.z[j];
    }
    cut_expression += cut.constant;
    first_stage.model.AddLinearConstraint(cut_expression <=
                                          cut.w_coefficient * first_stage.w);
    best_upper_bound = std::min(upper_bound, best_upper_bound);
    LOG(INFO) << "UB = " << best_upper_bound;
    ++iteration;
    if (best_upper_bound - lower_bound < target_precission) {
      std::cout << "Total iterations = " << iteration << std::endl;
      std::cout << "Final LB = " << absl::StrFormat("%.9f", lower_bound)
                << std::endl;
      std::cout << "Final UB = " << absl::StrFormat("%.9f", best_upper_bound)
                << std::endl;
      break;
    }
    if (iteration > maximum_iterations) {
      break;
    }
  }
  return absl::OkStatus();
}
absl::Status Main() {
  const FacilityLocationInstance instance{
      .network = Network(absl::GetFlag(FLAGS_num_facilities),
                         absl::GetFlag(FLAGS_num_locations),
                         absl::GetFlag(FLAGS_edge_probability)),
      .location_demand = absl::GetFlag(FLAGS_location_demand),
      .facility_cost = absl::GetFlag(FLAGS_facility_cost),
      .location_fraction = absl::GetFlag(FLAGS_location_fraction)};
  absl::Time start = absl::Now();
  RETURN_IF_ERROR(FullProblem(instance, absl::GetFlag(FLAGS_solver_type)))
      << "full solve failed";
  std::cout << "Full solve time: " << absl::Now() - start << std::endl;
  start = absl::Now();
  RETURN_IF_ERROR(Benders(instance, absl::GetFlag(FLAGS_benders_precission),
                          absl::GetFlag(FLAGS_solver_type)))
      << "Benders solve failed";
  std::cout << "Benders solve time: " << absl::Now() - start << std::endl;
  return absl::OkStatus();
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
