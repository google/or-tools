// Copyright 2011-2014 Google
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
//
//
//  Common base for routing solution classes.
//  A route is given by its depot (first node) and then the path (except for the first node), e.g.
//  RoutingSolution 1 -> 2 -> 5 is in fact path 1 -> 2 -> 5 -> 1 and 1 is a depot.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_SOLUTION_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_SOLUTION_H

#include "constraint_solver/routing.h"

#include "routing_data.h"

namespace operations_research {

  class RoutingSolution {
  public:
    explicit RoutingSolution(const RoutingData & data) : size_(data.Size()) {}
    virtual ~RoutingSolution() {}
    virtual bool IsSolution() const = 0;
    virtual bool IsFeasibleSolution() const = 0;
    virtual int64 ComputeObjectiveValue() const = 0;

    void SetSize(int32 size) {
      size_ = size;
    }
    //  Size of the instance.
    int32 Size() const {
      return size_;
    }
    virtual bool Add(RoutingModel::NodeIndex i, int route_number = 0) = 0; 
    virtual void Print(std::ostream & out) const = 0;
  protected:
    int32 size_;
  };
  
}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_SOLUTION_H