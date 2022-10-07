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

// Common utilities for parsing routing instances.

#ifndef OR_TOOLS_ROUTING_SIMPLE_GRAPH_H_
#define OR_TOOLS_ROUTING_SIMPLE_GRAPH_H_

#include <algorithm>
#include <functional>
#include <ostream>
#include <string>

#include "absl/hash/hash.h"

namespace operations_research {

class Arc;

// Edge, undirected, between the head to the tail.
// With a few bells and whistles to allow its use within hash tables.
class Edge {
 public:
  Edge(int64_t tail, int64_t head) : tail_(tail), head_(head) {}
  explicit Edge(const Arc& arc);

  int64_t tail() const { return tail_; }
  int64_t head() const { return head_; }

  bool operator==(const Edge& other) const {
    return (head_ == other.head_ && tail_ == other.tail_) ||
           (head_ == other.tail_ && tail_ == other.head_);
  }

  bool operator!=(const Edge& other) const { return !this->operator==(other); }

  template <typename H>
  friend H AbslHashValue(H h, const Edge& a) {
    // This hash value should not depend on the direction of the edge, hence
    // the use of min and max.
    return H::combine(std::move(h), std::min(a.head_, a.tail_),
                      std::max(a.head_, a.tail_));
  }

 private:
  const int64_t tail_;
  const int64_t head_;
};

// Arc, directed, from the tail to the head.
// With a few bells and whistles to allow its use within hash tables.
class Arc {
 public:
  Arc(int64_t tail, int64_t head) : tail_(tail), head_(head) {}
  explicit Arc(const Edge& edge);

  int64_t tail() const { return tail_; }
  int64_t head() const { return head_; }
  Arc reversed() const { return {head_, tail_}; }

  bool operator==(const Arc& other) const {
    return head_ == other.head_ && tail_ == other.tail_;
  }

  bool operator!=(const Arc& other) const { return !this->operator==(other); }

  template <typename H>
  friend H AbslHashValue(H h, const Arc& a) {
    // Unlike the edge, this value *must* depend on the direction of the arc.
    return H::combine(std::move(h), a.tail_, a.head_);
  }

 private:
  const int64_t tail_;
  const int64_t head_;
};

// Mapping between an edge (given by its tail and its head) and its weight.
typedef std::function<int64_t(int, int)> EdgeWeights;

// Real-world coordinates.
template <typename T>
struct Coordinates2 {
  T x = {};
  T y = {};

  Coordinates2() = default;
  Coordinates2(T x, T y) : x(x), y(y) {}

  friend bool operator==(const Coordinates2& a, const Coordinates2& b) {
    return a.x == b.x && a.y == b.y;
  }
  friend bool operator!=(const Coordinates2& a, const Coordinates2& b) {
    return !(a == b);
  }
  friend std::ostream& operator<<(std::ostream& stream,
                                  const Coordinates2& coordinates) {
    return stream << "{x = " << coordinates.x << ", y = " << coordinates.y
                  << "}";
  }
  template <typename H>
  friend H AbslHashValue(H h, const Coordinates2& coordinates) {
    return H::combine(std::move(h), coordinates.x, coordinates.y);
  }
};

template <typename T>
struct Coordinates3 {
  T x = {};
  T y = {};
  T z = {};

  Coordinates3() = default;
  Coordinates3(T x, T y, T z) : x(x), y(y), z(z) {}

  friend bool operator==(const Coordinates3& a, const Coordinates3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }
  friend bool operator!=(const Coordinates3& a, const Coordinates3& b) {
    return !(a == b);
  }
  friend std::ostream& operator<<(std::ostream& stream,
                                  const Coordinates3& coordinates) {
    return stream << "{x = " << coordinates.x << ", y = " << coordinates.y
                  << ", z = " << coordinates.z << "}";
  }
  template <typename H>
  friend H AbslHashValue(H h, const Coordinates3& coordinates) {
    return H::combine(std::move(h), coordinates.x, coordinates.y,
                      coordinates.z);
  }
};

// Time window, typically used for a node.
// Name chosen to avoid clash with tour_optimization.proto, defining a
// TimeWindow message with more fields.
template <typename T>
struct SimpleTimeWindow {
  T start;
  T end;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_SIMPLE_GRAPH_H_
