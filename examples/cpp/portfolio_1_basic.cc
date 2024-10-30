#include <fstream>
#include <iostream>
#include <ostream>
#include <limits>
#include <sstream>

#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"

const double inf = std::numeric_limits<double>::infinity();
namespace math_opt = ::operations_research::math_opt;
using math_opt::LinearExpression;


/// Build and solve a basic Markowit portfolio problem.
///
/// # Arguments 
/// - mu The vector of length n of expected returns on the assets
/// - GT A vector defining a m x n matrix in row-major format. It is the 
///   facored co-variance matrix, i.e. the covariance matrix is Q = G*G^T
/// - x0 A vector of length n of initial investments
/// - w Initial wealth not invested yet.
/// - gamma The risk bound as a bound on the variance of the return of portfolio, 
///   gamma > sqrt( xGG^Tx ).
absl::StatusOr<std::tuple<double,std::vector<double>>> basic_markowitz(
    const std::vector<double> & mu,
    const std::vector<double> & GT,
    const std::vector<double> & x0,
    double w,
    double gamma) {
   
  math_opt::Model model("portfolio_1_basic");
  size_t n = mu.size();
  size_t m = GT.size()/n;
  std::vector<math_opt::Variable> x; x.reserve(n);
  for (int i = 0; i < n; ++i)
    x.push_back(model.AddContinuousVariable(0.0,inf,(std::ostringstream() << "x" << i).str()));

  model.Maximize(LinearExpression::InnerProduct(x,mu));

  model.AddLinearConstraint(LinearExpression::Sum(x) == 0.0, "Budget");

  std::vector<math_opt::LinearExpression> linear_to_norm;
  for (int i = 0; i < m; ++i) {
    linear_to_norm.push_back(LinearExpression::InnerProduct(absl::Span(GT.data()+m*i,m), x));
  }
  model.AddSecondOrderConeConstraint(linear_to_norm, gamma, "risk"); 
    
  // Set parameters, e.g. turn on logging.
  math_opt::SolveArguments args;
  args.parameters.enable_output = true;

  // Solve and ensure an optimal solution was found with no errors.
  const absl::StatusOr<math_opt::SolveResult> result =
      math_opt::Solve(model, math_opt::SolverType::kMosek, args);
  if (! result.ok())
    return result.status();

  double pobj = result->objective_value();
  std::vector<double> res(n);
  for (int i = 0; i < n; ++i)
    res[i] = result->variable_values().at(x[i]);

  return std::make_tuple(pobj,std::move(res));
}

int main(int argc, char ** argv) {

    const int        n      = 8;
    const double     w      = 59.0;
    std::vector<double> mu = {0.07197349, 0.15518171, 0.17535435, 0.0898094 , 0.42895777, 0.39291844, 0.32170722, 0.18378628};
    std::vector<double> x0 = {8.0, 5.0, 3.0, 5.0, 2.0, 9.0, 3.0, 6.0};
    double gamma = 36;
    std::vector<double> GT = {
        0.30758, 0.12146, 0.11341, 0.11327, 0.17625, 0.11973, 0.10435, 0.10638,
        0.     , 0.25042, 0.09946, 0.09164, 0.06692, 0.08706, 0.09173, 0.08506,
        0.     , 0.     , 0.19914, 0.05867, 0.06453, 0.07367, 0.06468, 0.01914,
        0.     , 0.     , 0.     , 0.20876, 0.04933, 0.03651, 0.09381, 0.07742,
        0.     , 0.     , 0.     , 0.     , 0.36096, 0.12574, 0.10157, 0.0571 ,
        0.     , 0.     , 0.     , 0.     , 0.     , 0.21552, 0.05663, 0.06187,
        0.     , 0.     , 0.     , 0.     , 0.     , 0.     , 0.22514, 0.03327,
        0.     , 0.     , 0.     , 0.     , 0.     , 0.     , 0.     , 0.2202 };
  

    auto res = basic_markowitz(mu,GT,x0,w,gamma);

    if (! res.ok()) {
      std::cerr << "Failed to solve problem" << std::endl;
    }
    else {
      auto &[pobj,xx] = *res;
      std::cout << "Primal Objective value: " << pobj << std::endl;
      std::cout << "Solution values:" << std::endl;
      for (int i = 0; i < xx.size(); ++i)
        std::cout << "  x["<< i << "] = " << xx[i] << std::endl;
    }
}
