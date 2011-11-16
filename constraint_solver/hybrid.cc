// Copyright 2010-2011 Google
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

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "constraint_solver/constraint_solver.h"
#include "linear_solver/linear_solver.h"

DEFINE_int32(simplex_cleanup_frequency, 0,
             "frequency to cleanup the simplex after each call, 0: no cleanup");
DEFINE_bool(verbose_simplex_call, false,
            "Do not suppress output of the simplex");
DEFINE_bool(use_clp, true, "use Clp instead of glpk");

namespace operations_research {
namespace {
MPSolver::OptimizationProblemType GetType(bool use_clp) {
  if (use_clp) {
#if defined(USE_CLP)
    return MPSolver::CLP_LINEAR_PROGRAMMING;
#else
    LOG(FATAL) << "CLP not defined";
#endif  // USE_CLP
  } else {
#if defined(USE_GLPK)
    return MPSolver::GLPK_LINEAR_PROGRAMMING;
#else
    LOG(FATAL) << "GLPK not defined";
#endif  // USE_GLPK
  }
}

class SimplexConstraint : public SearchMonitor {
 public:
  SimplexConstraint(Solver* const solver,
                    Callback1<MPSolver*>* const builder,
                    Callback1<MPSolver*>* const modifier,
                    Callback1<MPSolver*>* const runner,
                    int simplex_frequency)
      : SearchMonitor(solver),
        builder_(builder),
        modifier_(modifier),
        runner_(runner),
        mp_solver_("InSearchSimplex", GetType(FLAGS_use_clp)),
        counter_(0LL),
        simplex_frequency_(simplex_frequency) {
    if (builder != NULL) {
      builder->CheckIsRepeatable();
    }
    if (modifier != NULL) {
      modifier->CheckIsRepeatable();
    }
    if (runner != NULL) {
      runner->CheckIsRepeatable();
    }
    if (!FLAGS_verbose_simplex_call) {
      mp_solver_.SuppressOutput();
    }
  }

  virtual void EndInitialPropagation() {
    mp_solver_.Clear();
    if (builder_.get() != NULL) {
      builder_->Run(&mp_solver_);
    }
    RunOptim();
  }

  virtual void BeginNextDecision(DecisionBuilder* const b) {
    if (++counter_ % simplex_frequency_ == 0) {
      const int cleanup = FLAGS_simplex_cleanup_frequency * simplex_frequency_;
      if (FLAGS_simplex_cleanup_frequency != 0 && counter_ %  cleanup == 0) {
        mp_solver_.Clear();
        if (builder_.get() != NULL) {
          builder_->Run(&mp_solver_);
        }
      }
      RunOptim();
    }
  }

  void RunOptim() {
    if (modifier_.get() != NULL) {
      modifier_->Run(&mp_solver_);
    }
    if (runner_.get() != NULL) {
      runner_->Run(&mp_solver_);
    }
  }

 private:
  scoped_ptr<Callback1<MPSolver*> > builder_;
  scoped_ptr<Callback1<MPSolver*> > modifier_;
  scoped_ptr<Callback1<MPSolver*> > runner_;
  MPSolver mp_solver_;
  int64 counter_;
  const int simplex_frequency_;
  DISALLOW_COPY_AND_ASSIGN(SimplexConstraint);
};
}  // namespace

SearchMonitor* Solver::MakeSimplexConnection(
    Callback1<MPSolver*>* const builder,
    Callback1<MPSolver*>* const modifier,
    Callback1<MPSolver*>* const runner,
    int frequency) {
  return RevAlloc(new SimplexConstraint(this,
                                        builder,
                                        modifier,
                                        runner,
                                        frequency));
}

}  // namespace operations_research
