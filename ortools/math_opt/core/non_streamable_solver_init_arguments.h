// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_NON_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_CORE_NON_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_

#include <memory>

#include "ortools/math_opt/parameters.pb.h"

namespace operations_research {
namespace math_opt {

struct NonStreamableCpSatInitArguments;
struct NonStreamableGScipInitArguments;
struct NonStreamableGlopInitArguments;
struct NonStreamableGlpkInitArguments;
struct NonStreamableGurobiInitArguments;
struct NonStreamablePdlpInitArguments;

// Interface for solver specific parameters used at the solver instantiation
// that can't be streamed (for example instances of C/C++ types that only exist
// in the process memory).
//
// Since implementations of this interface usually depend on solver specific
// C/C++ types, they are in a dedicated header in the solver library.
//
// This class is the interface shared by the parameters of each solver, users
// should instantiate the solver specific class below.
//
// To enable safe cast of a pointer to this interface, there is an
// ToNonStreamableXxxInitArguments() function for each solver. Only one of these
// function will return a non-null value, depending on the type of the
// implementation class.
//
// Implementation should use NonStreamableSolverInitArgumentsHelper to
// automatically implements some methods.
struct NonStreamableSolverInitArguments {
  virtual ~NonStreamableSolverInitArguments() = default;

  // Returns the type of solver that the implementation is for.
  virtual SolverTypeProto solver_type() const = 0;

  // Returns this for the NonStreamableCpSatInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamableCpSatInitArguments*
  ToNonStreamableCpSatInitArguments() const {
    return nullptr;
  }

  // Returns this for the NonStreamableGScipInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamableGScipInitArguments*
  ToNonStreamableGScipInitArguments() const {
    return nullptr;
  }

  // Returns this for the NonStreamableGlopInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamableGlopInitArguments*
  ToNonStreamableGlopInitArguments() const {
    return nullptr;
  }

  // Returns this for the NonStreamableGlpkInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamableGlpkInitArguments*
  ToNonStreamableGlpkInitArguments() const {
    return nullptr;
  }

  // Returns this for the NonStreamableGurobiInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamableGurobiInitArguments*
  ToNonStreamableGurobiInitArguments() const {
    return nullptr;
  }

  // Returns this for the NonStreamablePdlpInitArguments class, nullptr for
  // other classes.
  virtual const NonStreamablePdlpInitArguments*
  ToNonStreamablePdlpInitArguments() const {
    return nullptr;
  }

  // Return a copy of this.
  //
  // The NonStreamableSolverInitArgumentsHelper implements this automatically
  // using the copy constructor (this base class is copyable intentionally).
  virtual std::unique_ptr<const NonStreamableSolverInitArguments> Clone()
      const = 0;
};

// Base struct for implementations that automatically implements solver_type()
// and Clone() virtual methods.
//
// The Clone() method is implemented with the copy constructor of the struct.
//
// All that is left to the implementation is to provide are the solver specific
// field and the implementation of the ToNonStreamableXxxInitArguments()
// corresponding to the solver type.
//
// Usage:
//
//   struct NonStreamableXxxInitArguments
//       : public NonStreamableSolverInitArgumentsHelper<
//             NonStreamableXxxInitArguments, SOLVER_TYPE_XXX> {
//
//     ... some data member here ...
//
//     const NonStreamableXxxInitArguments*
//       ToNonStreamableXxxInitArguments() const { return this; }
//   };
template <typename Implementation, SolverTypeProto impl_solver_type>
struct NonStreamableSolverInitArgumentsHelper
    : public NonStreamableSolverInitArguments {
  SolverTypeProto solver_type() const final { return impl_solver_type; }

  std::unique_ptr<const NonStreamableSolverInitArguments> Clone() const final {
    return std::make_unique<Implementation>(
        *static_cast<const Implementation*>(this));
  }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_NON_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_
