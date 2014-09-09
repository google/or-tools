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
//  Common routing stuff.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_H

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>

#include "base/random.h"
#include "constraint_solver/routing.h"

#include "routing_common/routing_common_flags.h"
#include "common/IO_helpers.h"
#include "common/constants.h"

namespace operations_research {


// Simple struct to contain points in the plane or in the space.
struct Point {
  Point(const double x_, const double y_, const double z_ = 0.0):
     x(x_), y(y_), z(z_){}
  Point(): x(-1.0), y(-1.0), z(-1.0){}
  double x;
  double y;
  double z;
};

//  Simple container class to hold cost on arcs of a complete graph.
//  This class doesn't compute the distances but only stores them.
//  IsCreated(): the cost/distance matrix exists;
//  IsInstanciated(): the matrix is filled.
//
//  Distances/costs can be symetric or not.
class CompleteGraphArcCost {
public:
  explicit CompleteGraphArcCost(int32 size = 0): size_(size), is_created_(false), is_instanciated_(false), is_symmetric_(false),
    min_cost_(kPostiveInfinityInt64), max_cost_(-1) {
    if (size_ > 0) {
      CreateMatrix(size_);
    }
  }

  int32 Size() const {
    return size_;
  }

  void Create(int32 size) {
    CHECK(!IsCreated()) << "Matrix already created!";
    size_ = size;
    CreateMatrix(size);
  }

  bool IsCreated() const {
    return is_created_;
  }

  bool IsInstanciated() const {
    return is_instanciated_;
  }

  void SetIsInstanciated(const bool instanciated = true) {
    CHECK(IsCreated()) << "Instance is not created!";
    is_instanciated_ = instanciated;
    ComputeExtremeDistance();
    ComputeIsSymmetric();
  }
  
  int64 Cost(RoutingModel::NodeIndex from,
                   RoutingModel::NodeIndex to) const {
    return matrix_[MatrixIndex(from, to)];
  }

  int64& Cost(RoutingModel::NodeIndex from,
                   RoutingModel::NodeIndex to) {
    return matrix_[MatrixIndex(from, to)];
  }
  
  int64 MaxCost() const {
    CHECK(IsInstanciated()) << "Instance is not instanciated!";
    return max_cost_;
  }
  
  int64 MinCost() const {
    CHECK(IsInstanciated()) << "Instance is not instanciated!";
    return min_cost_;
  }

  bool IsSymmetric() const {
    CHECK(IsInstanciated()) << "Instance is not instanciated!";
    return is_symmetric_;
  }

 
  void Print(std::ostream & out, const bool label = false, const int width = FLAGS_width_size) const;

private:
  int64 MatrixIndex(RoutingModel::NodeIndex from,
                    RoutingModel::NodeIndex to) const {
    return (from * size_ + to).value();
  }

  void CreateMatrix(const int size) {
    CHECK_GT(size, 2) << "Size for matrix non consistent.";
    int64 * p_array = NULL;
    try {
      p_array  = new int64 [size_ * size_];
    } catch (std::bad_alloc & e) {
      p_array = NULL;
      LOG(FATAL) << "Problems allocating ressource. Try with a smaller size.";
    }
    CHECK_NE(p_array, NULL) << "Not enough resources to create matrix";
    matrix_.reset(p_array);
    is_created_ = true;
  }

  void UpdateExtremeDistance(int64 dist) {
    if (min_cost_ > dist) { min_cost_ = dist;}
    if (max_cost_ < dist) { max_cost_ = dist;}
  }

 void ComputeExtremeDistance() {
    CHECK(IsInstanciated()) << "Instance is not instanciated!";
    min_cost_ = kPostiveInfinityInt64;
    max_cost_ = -1;
    for (RoutingModel::NodeIndex i(0); i < size_; ++i) {
      for (RoutingModel::NodeIndex j(0); j < size_; ++j) {
        if (i == j) {continue;}
        UpdateExtremeDistance(Cost(i,j));
      }
    }
  }
  
  bool ComputeIsSymmetric() {
    CHECK(IsInstanciated()) << "Instance is not instanciated!";
    for (RoutingModel::NodeIndex i(0); i < Size(); ++i) {
      for (RoutingModel::NodeIndex j(i + 1); j < Size(); ++j) {
        if (matrix_[MatrixIndex(i,j)] != matrix_[MatrixIndex(j,i)]) {
          return false;
        }
      }
    }

    return true;
  }

  int32 size_;
  std::unique_ptr<int64[]> matrix_;

  bool is_created_;
  bool is_instanciated_;
  bool is_symmetric_;
  int64 min_cost_;
  int64 max_cost_;
};

void CompleteGraphArcCost::Print(std::ostream& out, const bool label, const int width) const {
  CHECK(IsInstanciated()) << "Instance is not instanciated!";
  //  titel
  out.width(width);
  if (label) {
    out << std::left << " ";
    for (RoutingModel::NodeIndex to = RoutingModel::kFirstNode; to < size_; ++to) {
      out.width(width);
      out << std::right << to.value() + 1 ;
    }
    out << std::endl;
  }
  //  fill
  for (RoutingModel::NodeIndex from = RoutingModel::kFirstNode; from < size_; ++from) {
    if (label) {
      out.width(width);
      out << std::right << from.value() + 1;
    }
    for (RoutingModel::NodeIndex to = RoutingModel::kFirstNode; to < size_; ++to) {
      out.width(width);
      out << std::right << matrix_[MatrixIndex(from, to)];
    }
    out << std::endl;
  }
}

struct BoundingBox {

  BoundingBox(): min_x(std::numeric_limits<double>::max()),
                 max_x(std::numeric_limits<double>::min()),
                 min_y(std::numeric_limits<double>::max()),
                 max_y(std::numeric_limits<double>::min()),
                 min_z(std::numeric_limits<double>::max()),
                 max_z(std::numeric_limits<double>::min()) {}
  BoundingBox(double min_x_, double max_x_, double min_y_, double max_y_, double min_z_, double max_z_):
    min_x(min_x_), max_x(max_x_), min_y(min_y_), max_y(max_y_),  min_z(min_z_), max_z(max_z_) {}

  void Update(Point p) {
    if (p.x < min_x) {min_x = p.x;}
    if (p.x > max_x) {max_x = p.x;}
    if (p.y < min_y) {min_y = p.y;}
    if (p.y > max_y) {max_y = p.y;}
    if (p.z < min_z) {min_z = p.z;}
    if (p.z > max_z) {max_z = p.z;}
  }
  // Bounding box
  double min_x;
  double max_x;
  double min_y;
  double max_y;
  double min_z;
  double max_z;
};

}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_H