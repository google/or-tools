// Copyright 2010-2021 Google LLC
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

// Simple linear programming example

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/random/random.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(int, num_facilities, 3000, "Number of facilities.");
ABSL_FLAG(int, num_locations, 50, "Number of locations.");
ABSL_FLAG(double, edge_probability, 0.99, "Edge probability.");
ABSL_FLAG(double, benders_precission, 1e-9, "Benders target precission.");
ABSL_FLAG(double, location_demand, 1, "Client demands.");
ABSL_FLAG(double, facility_cost, 100, "Facility capacity cost.");
ABSL_FLAG(
    double, location_fraction, 0.001,
    "Fraction of a facility's capacity that can be used by each location.");

namespace {
using ::operations_research::math_opt::GurobiParametersProto;
using ::operations_research::math_opt::LinearConstraint;
using ::operations_research::math_opt::LinearExpression;
using ::operations_research::math_opt::MathOpt;
using ::operations_research::math_opt::Objective;
using ::operations_research::math_opt::Result;
using ::operations_research::math_opt::SolveParametersProto;
using ::operations_research::math_opt::SolveResultProto;
using ::operations_research::math_opt::Sum;
using ::operations_research::math_opt::Variable;

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
  absl::BitGen bitgen;

  num_facilities_ = num_facilities;
  num_locations_ = num_locations;
  facility_edge_incidence_ =
      std::vector<std::vector<std::pair<int, int>>>(num_facilities);
  location_edge_incidence_ = std::vector<std::vector<std::pair<int, int>>>(
      num_locations, std::vector<std::pair<int, int>>());
  for (int facility = 0; facility < num_facilities_; ++facility) {
    for (int location = 0; location < num_locations_; ++location) {
      if (absl::Bernoulli(bitgen, edge_probability)) {
        const std::pair<int, int> edge({facility, location});
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

constexpr double kInf = std::numeric_limits<double>::infinity();

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
//   min c * sum(z_f : f in F) + sum(h_e * x_e : e in E)
//   s.t.
//                                   x_(f,l) <= a * z_f   for all (f,l) in E
//     sum(x_(f,l) : l such that (f,l) in E) <=     z_f   for all f in F
//     sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
//                                       x_e >= 0     for all e in E
//                                       z_f >= 0     for all f in F
//
void FullProblem(const Network& network, const double location_demand,
                 const double facility_cost, const double location_fraction) {
  const int num_facilities = network.num_facilities();
  const int num_locations = network.num_locations();

  MathOpt model(operations_research::math_opt::SOLVER_TYPE_GUROBI,
                "Full network design problem");
  const Objective objective = model.objective();
  objective.set_minimize();

  // Capacity variables
  std::vector<Variable> z;
  for (int j = 0; j < num_facilities; j++) {
    const Variable z_j = model.AddContinuousVariable(0.0, kInf);
    z.push_back(z_j);
    objective.set_linear_coefficient(z_j, facility_cost);
  }

  // Flow variables
  absl::flat_hash_map<Edge, Variable> x;
  for (const auto& edge : network.edges()) {
    const Variable x_edge = model.AddContinuousVariable(0.0, kInf);
    x.insert({edge, x_edge});
    objective.set_linear_coefficient(x_edge, network.edge_cost(edge));
  }

  // Demand constraints
  for (int location = 0; location < num_locations; ++location) {
    LinearExpression linear_expression;
    for (const auto& edge : network.edges_incident_to_location(location)) {
      linear_expression += x.at(edge);
    }
    model.AddLinearConstraint(linear_expression >= location_demand);
  }

  // Supply constraints
  for (int facility = 0; facility < num_facilities; ++facility) {
    LinearExpression linear_expression;
    for (const auto& edge : network.edges_incident_to_facility(facility)) {
      linear_expression += x.at(edge);
    }
    model.AddLinearConstraint(linear_expression <= z[facility]);
  }

  // Arc constraints
  for (int facility = 0; facility < num_facilities; ++facility) {
    for (const auto& edge : network.edges_incident_to_facility(facility)) {
      model.AddLinearConstraint(x.at(edge) <= location_fraction * z[facility]);
    }
  }
  const Result result = model.Solve(SolveParametersProto()).value();
  for (const auto& warning : result.warnings) {
    LOG(WARNING) << "Solver warning: " << warning << std::endl;
  }
  QCHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
      << "Failed to find an optimal solution: " << result.termination_detail;
  std::cout << "Full problem optimal objective: "
            << absl::StrFormat("%.9f", result.objective_value()) << std::endl;
}

void Benders(const Network network, const double location_demand,
             const double facility_cost, const double location_fraction,
             const double target_precission,
             const int maximum_iterations = 30000) {
  const int num_facilities = network.num_facilities();
  const int num_locations = network.num_locations();

  // Setup first stage model.
  //
  //   min c * sum(z_f : f in F) + w
  //   s.t.
  //                                       z_f >= 0     for all f in F
  //          sum(fcut_f^i z_f) + fcut_const^i <= 0      for i = 1,...
  //          sum(ocut_f^j z_f) + ocut_const^j <= w      for j = 1,...
  MathOpt first_stage_model(operations_research::math_opt::SOLVER_TYPE_GUROBI,
                            "First stage problem");
  std::vector<Variable> z;
  for (int j = 0; j < num_facilities; j++) {
    z.push_back(first_stage_model.AddContinuousVariable(0.0, kInf));
  }

  const Variable w = first_stage_model.AddContinuousVariable(0.0, kInf);

  first_stage_model.objective().Minimize(facility_cost * Sum(z) + w);

  SolveParametersProto first_stage_params;
  first_stage_params.mutable_common_parameters()->set_enable_output(false);

  // Setup second stage model.
  //   min sum(h_e * x_e : e in E)
  //   s.t.
  //                                   x_(f,l) <= a * zz_f   for all (f,l) in E
  //     sum(x_(f,l) : l such that (f,l) in E) <=     zz_f   for all f in F
  //     sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
  //                                       x_e >= 0     for all e in E
  //
  // where zz_f are fixed values for z_f from the first stage model.
  MathOpt second_stage_model(operations_research::math_opt::SOLVER_TYPE_GUROBI,
                             "Second stage model");
  const Objective second_stage_objective = second_stage_model.objective();
  second_stage_objective.set_minimize();
  absl::flat_hash_map<Edge, Variable> x;
  for (const auto& edge : network.edges()) {
    const Variable x_edge = second_stage_model.AddContinuousVariable(0.0, kInf);
    x.insert({edge, x_edge});
    second_stage_objective.set_linear_coefficient(x_edge,
                                                  network.edge_cost(edge));
  }
  std::vector<LinearConstraint> demand_constraints;

  for (int location = 0; location < num_locations; ++location) {
    LinearExpression linear_expression;
    for (const auto& edge : network.edges_incident_to_location(location)) {
      linear_expression += x.at(edge);
    }
    demand_constraints.push_back(second_stage_model.AddLinearConstraint(
        linear_expression >= location_demand));
  }
  std::vector<LinearConstraint> supply_constraints;
  for (int facility = 0; facility < num_facilities; ++facility) {
    LinearExpression linear_expression;
    for (const auto& edge : network.edges_incident_to_facility(facility)) {
      linear_expression += x.at(edge);
    }
    supply_constraints.push_back(
        second_stage_model.AddLinearConstraint(linear_expression <= kInf));
  }

  SolveParametersProto second_stage_params;
  second_stage_params.mutable_common_parameters()->set_enable_output(false);
  GurobiParametersProto::Parameter* param1 =
      second_stage_params.mutable_gurobi_parameters()->add_parameters();
  param1->set_name("InfUnbdInfo");
  param1->set_value("1");

  // Start Benders
  int iteration = 0;
  double best_upper_bound = kInf;
  while (true) {
    LOG(INFO) << "Iteration: " << iteration;
    // Solve and process first stage.
    const Result first_stage_result =
        first_stage_model.Solve(first_stage_params).value();
    for (const auto& warning : first_stage_result.warnings) {
      LOG(WARNING) << "Solver warning: " << warning << std::endl;
    }
    QCHECK_EQ(first_stage_result.termination_reason, SolveResultProto::OPTIMAL);
    const double lower_bound = first_stage_result.objective_value();
    LOG(INFO) << "LB = " << lower_bound;

    // Setup second stage.
    for (int facility = 0; facility < num_facilities; ++facility) {
      const double capacity_value =
          first_stage_result.variable_values().at(z[facility]);
      for (const auto& edge : network.edges_incident_to_facility(facility)) {
        x.at(edge).set_upper_bound(location_fraction * capacity_value);
      }
      supply_constraints[facility].set_upper_bound(capacity_value);
    }

    // Solve and process second stage.
    const Result second_stage_result =
        second_stage_model.Solve(second_stage_params).value();
    for (const auto& warning : second_stage_result.warnings) {
      LOG(WARNING) << "Solver warning: " << warning << std::endl;
    }
    if (second_stage_result.termination_reason ==
        SolveResultProto::INFEASIBLE) {
      // If the second stage problem is infeasible we will get a dual ray
      // (r, y) such that
      //
      // sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
      // + sum(y_f*zz_f : f in F, y_f < 0)
      // + sum(y_l*d : l in L, y_l > 0) > 0.
      //
      // Then we get the feasibility cut (go/mathopt-advanced-dual-use#benders)
      //
      // sum(fcut_f*z_f) + fcut_const <= 0,
      //
      // where
      //
      // fcut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
      //              + min{y_f, 0}
      // fcut_const = sum*(y_l*d : l in L, y_l > 0)
      LOG(INFO) << "Adding feasibility cut...";
      LinearExpression feasibility_cut_expression;
      for (int facility = 0; facility < num_facilities; ++facility) {
        double coefficient = 0.0;
        for (const auto& edge : network.edges_incident_to_facility(facility)) {
          const double reduced_cost =
              second_stage_result.ray_reduced_costs().at(x.at(edge));
          if (reduced_cost < 0) {
            coefficient += location_fraction * reduced_cost;
          }
        }
        double dual_value = second_stage_result.ray_dual_values().at(
            supply_constraints[facility]);
        coefficient += std::min(dual_value, 0.0);
        feasibility_cut_expression += coefficient * z[facility];
      }
      double constant = 0.0;
      for (const auto& constraint : demand_constraints) {
        double dual_value =
            second_stage_result.ray_dual_values().at(constraint);
        if (dual_value > 0) {
          constant += dual_value;
        }
      }
      first_stage_model.AddLinearConstraint(
          feasibility_cut_expression + constant <= 0.0);
    } else {
      // If the second stage problem is optimal we will get a dual solution
      // (r, y) such that the optimal objective value is equal to
      //
      // sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
      // + sum(y_f*zz_f : f in F, y_f < 0)
      // + sum*(y_l*d : l in L, y_l > 0) > 0.
      //
      // Then we get the optimality cut (go/mathopt-advanced-dual-use#benders)
      //
      // sum(ocut_f*z_f) + ocut_const <= w,
      //
      // where
      //
      // ocut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
      //              + min{y_f, 0}
      // ocut_const = sum*(y_l*d : l in L, y_l > 0)
      QCHECK_EQ(second_stage_result.termination_reason,
                SolveResultProto::OPTIMAL);
      LOG(INFO) << "Adding optimality cut...";
      LinearExpression optimality_cut_expression;
      double upper_bound = 0.0;
      for (int facility = 0; facility < num_facilities; ++facility) {
        double coefficient = 0.0;
        for (const auto& edge : network.edges_incident_to_facility(facility)) {
          upper_bound += network.edge_cost(edge) *
                         second_stage_result.variable_values().at(x.at(edge));
          const double reduced_cost =
              second_stage_result.reduced_costs().at(x.at(edge));
          if (reduced_cost < 0) {
            coefficient += location_fraction * reduced_cost;
          }
        }
        double dual_value =
            second_stage_result.dual_values().at(supply_constraints[facility]);
        coefficient += std::min(dual_value, 0.0);
        optimality_cut_expression += coefficient * z[facility];
      }
      double constant = 0.0;
      for (const auto& constraint : demand_constraints) {
        double dual_value = second_stage_result.dual_values().at(constraint);
        if (dual_value > 0) {
          constant += dual_value;
        }
      }
      // Add first stage contribution to upper bound = facility_cost * Sum(z)
      upper_bound += first_stage_result.objective_value() -
                     first_stage_result.variable_values().at(w);
      best_upper_bound = std::min(best_upper_bound, upper_bound);

      first_stage_model.AddLinearConstraint(
          optimality_cut_expression + constant <= w);
    }
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
}

}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  const Network network(absl::GetFlag(FLAGS_num_facilities),
                        absl::GetFlag(FLAGS_num_locations),
                        absl::GetFlag(FLAGS_edge_probability));
  absl::Time start = absl::Now();
  FullProblem(network, absl::GetFlag(FLAGS_location_demand),
              absl::GetFlag(FLAGS_facility_cost),
              absl::GetFlag(FLAGS_location_fraction));
  std::cout << "Full solve time : " << absl::Now() - start << std::endl;
  start = absl::Now();
  Benders(network, absl::GetFlag(FLAGS_location_demand),
          absl::GetFlag(FLAGS_facility_cost),
          absl::GetFlag(FLAGS_location_fraction),
          absl::GetFlag(FLAGS_benders_precission));
  std::cout << "Benders solve time : " << absl::Now() - start << std::endl;
  return 0;
}
