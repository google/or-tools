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

#ifndef PDLP_SHARDER_H_
#define PDLP_SHARDER_H_

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "ortools/base/threadpool.h"

namespace operations_research::pdlp {

// This class represents a way to shard elements for multi-threading. The
// elements may be entries of a vector or the columns of a (column-major)
// matrix. The shards are selected to have roughly the same mass, where the mass
// of an entry depends on the constructor used. See the free functions below and
// in the .cc file for example usage.
class Sharder {
 public:
  // These are public aliases for convenience. They will change only if there
  // are breaking changes in Eigen.
  using ConstSparseColumnBlock = ::Eigen::Block<
      const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>,
      /*BlockRows=*/Eigen::Dynamic, /*BlockCols=*/Eigen::Dynamic,
      /*InnerPanel=*/true>;
  using SparseColumnBlock =
      ::Eigen::Block<Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>,
                     /*BlockRows=*/Eigen::Dynamic, /*BlockCols=*/Eigen::Dynamic,
                     /*InnerPanel=*/true>;

  // This class extracts a particular shard of vectors or matrices passed to it.
  // See `ParallelForEachShard()`.
  // Caution: Like `absl::Span`, `Shard::operator()` returns mutable or
  // immutable views into the vector or matrix argument. The underlying object
  // must outlive the view.
  // Extra Caution: The const& arguments for the immutable views can bind to
  // temporary objects, e.g., shard(3*a) will create a view into the "3*a"
  // object that will be destroyed immediately after the shard is created.
  class Shard {
   public:
    // Returns this shard of `vector`.
    Eigen::VectorBlock<const Eigen::VectorXd> operator()(
        const Eigen::VectorXd& vector) const {
      CHECK_EQ(vector.size(), parent_.NumElements());
      return vector.segment(parent_.ShardStart(shard_num_),
                            parent_.ShardSize(shard_num_));
    }
    // Returns this shard of `vector` in mutable form.
    Eigen::VectorBlock<Eigen::VectorXd> operator()(
        Eigen::VectorXd& vector) const {
      CHECK_EQ(vector.size(), parent_.NumElements());
      return vector.segment(parent_.ShardStart(shard_num_),
                            parent_.ShardSize(shard_num_));
    }
    // Returns this shard of `vector`.
    Eigen::VectorBlock<const Eigen::VectorXd> operator()(
        Eigen::VectorBlock<const Eigen::VectorXd> vector) const {
      CHECK_EQ(vector.size(), parent_.NumElements());
      return Eigen::VectorBlock<const Eigen::VectorXd>(
          vector.nestedExpression(),
          vector.startRow() + parent_.ShardStart(shard_num_),
          parent_.ShardSize(shard_num_));
    }
    // Returns this shard of `vector` in mutable form.
    Eigen::VectorBlock<Eigen::VectorXd> operator()(
        Eigen::VectorBlock<Eigen::VectorXd> vector) const {
      CHECK_EQ(vector.size(), parent_.NumElements());
      return Eigen::VectorBlock<Eigen::VectorXd>(
          vector.nestedExpression(),
          vector.startRow() + parent_.ShardStart(shard_num_),
          parent_.ShardSize(shard_num_));
    }
    // Returns this shard of `diag`. Note that the shard is a *square* diagonal
    // matrix, not a block of columns of original length.
    auto operator()(const Eigen::DiagonalMatrix<double, Eigen::Dynamic>& diag)
        const -> decltype(diag.diagonal().segment(0, 0).asDiagonal()) {
      CHECK_EQ(diag.diagonal().size(), parent_.NumElements());
      return diag.diagonal()
          .segment(parent_.ShardStart(shard_num_),
                   parent_.ShardSize(shard_num_))
          .asDiagonal();
    }
    // Returns this shard of the columns of `matrix`.
    ConstSparseColumnBlock operator()(
        const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix)
        const {
      CHECK_EQ(matrix.cols(), parent_.NumElements());
      auto result = matrix.middleCols(parent_.ShardStart(shard_num_),
                                      parent_.ShardSize(shard_num_));
      // This is a guard against implicit conversions, because the return type
      // of `middleCols` is not 100% clear from the documentation.
      static_assert(
          std::is_same<decltype(result), ConstSparseColumnBlock>::value,
          "The return type of middleCols changed!");
      return result;
    }
    // Returns this shard of the columns of `matrix` in mutable form.
    SparseColumnBlock operator()(
        Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix) const {
      CHECK_EQ(matrix.cols(), parent_.NumElements());
      auto result = matrix.middleCols(parent_.ShardStart(shard_num_),
                                      parent_.ShardSize(shard_num_));
      // This is a guard against implicit conversions, because the return type
      // of `middleCols` is not 100% clear from the documentation.
      static_assert(std::is_same<decltype(result), SparseColumnBlock>::value,
                    "The return type of middleCols changed!");
      return result;
    }
    // A non-negative identifier for this shard, less than `NumShards()` of the
    // parent `Sharder`.
    int Index() const { return shard_num_; }

   private:
    friend class Sharder;
    Shard(int shard_num, const Sharder* parent)
        : shard_num_(shard_num), parent_(*parent) {
      CHECK_NE(parent, nullptr);
      CHECK_GE(shard_num, 0);
      CHECK_LT(shard_num, parent->NumShards());
    }
    const int shard_num_;
    const Sharder& parent_;
  };

  // Creates a `Sharder` for problems with `num_elements` elements and mass of
  // each element given by `element_mass`. Each shard will have roughly the same
  // mass. The number of shards in the resulting `Sharder` will be approximately
  // `num_shards` but may differ. The `thread_pool` will be used for parallel
  // operations executed by e.g. `ParallelForEachShard()`. The `thread_pool` may
  // be nullptr, which means work will be executed in the same thread. If
  // `thread_pool` is not nullptr, the underlying object is not owned and must
  // outlive the `Sharder`.
  Sharder(int64_t num_elements, int num_shards, ThreadPool* thread_pool,
          const std::function<int64_t(int64_t)>& element_mass);

  // Creates a `Sharder` for problems with `num_elements` elements and unit
  // mass. This constructor exploits having all element mass equal to 1 to take
  // time proportional to `num_shards` instead of `num_elements`. Also see the
  // comments above the first constructor.
  Sharder(int64_t num_elements, int num_shards, ThreadPool* thread_pool);

  // Creates a `Sharder` for processing `matrix`. The elements correspond to
  // columns of `matrix` and have mass linear in the number of non-zeros. Also
  // see the comments above the first constructor.
  Sharder(const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
          int num_shards, ThreadPool* thread_pool)
      : Sharder(matrix.cols(), num_shards, thread_pool, [&matrix](int64_t col) {
          return 1 + 1 * matrix.col(col).nonZeros();
        }) {}

  // Constructs a `Sharder` with the same thread pool as `other_sharder`, for
  // problems with `num_elements` elements and unit mass. The number of shards
  // will be approximately the same as that of `other_sharder`. Also see the
  // comments on the first constructor.
  Sharder(const Sharder& other_sharder, int64_t num_elements);

  // `Sharder` may be copied or moved.
  // Moved-from objects may be in an invalid state. The only methods that may be
  // called on a moved-from object are the destructor or `operator=`.
  Sharder(const Sharder& other) = default;
  Sharder(Sharder&& other) = default;
  Sharder& operator=(const Sharder& other) = default;
  Sharder& operator=(Sharder&& other) = default;

  int NumShards() const { return static_cast<int>(shard_starts_.size()) - 1; }

  // The number of elements that are split into shards.
  int64_t NumElements() const { return shard_starts_.back(); }

  int64_t ShardSize(int shard) const {
    CHECK_GE(shard, 0);
    CHECK_LT(shard, NumShards());
    return shard_starts_[shard + 1] - shard_starts_[shard];
  }

  int64_t ShardStart(int shard) const {
    CHECK_GE(shard, 0);
    CHECK_LT(shard, NumShards());
    return shard_starts_[shard];
  }

  int64_t ShardMass(int shard) const {
    CHECK_GE(shard, 0);
    CHECK_LT(shard, NumShards());
    return shard_masses_[shard];
  }

  // Runs `func` on each of the shards.
  void ParallelForEachShard(
      const std::function<void(const Shard&)>& func) const;

  // Runs `func` on each of the shards and sums the results.
  double ParallelSumOverShards(
      const std::function<double(const Shard&)>& func) const;

  // Runs `func` on each of the shards. Returns true iff all shards returned
  // true.
  bool ParallelTrueForAllShards(
      const std::function<bool(const Shard&)>& func) const;

  // Public for testing only.
  const std::vector<int64_t>& ShardStartsForTesting() const {
    return shard_starts_;
  }

 private:
  // Size: `NumShards() + 1`. The first entry is 0 and the last entry is
  // `NumElements()`. The entries are sorted in increasing order and are unique.
  // Note that {0} is valid and indicates zero elements split into zero shards.
  std::vector<int64_t> shard_starts_;
  // Size: `NumShards()`. The mass of each shard.
  std::vector<int64_t> shard_masses_;
  // NOT owned. May be nullptr.
  ThreadPool* thread_pool_;
};

// Like `matrix.transpose() * vector` but executed in parallel using `sharder`.
// The size of `sharder` must match the number of columns in `matrix`. To ensure
// good parallelization `matrix` should have (roughly) the same location of
// non-zeros as the `matrix` used when constructing `sharder`.
Eigen::VectorXd TransposedMatrixVectorProduct(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const Eigen::VectorXd& vector, const Sharder& sharder);

////////////////////////////////////////////////////////////////////////////////
// The following functions use `sharder` to compute a vector operation in
// parallel. `sharder` should have the same size as the vector(s). For best
// performance `sharder` should have been created with the `Sharder(int64_t,
// int, ThreadPool*)` constructor.
////////////////////////////////////////////////////////////////////////////////

// Like `dest.setZero(sharder.NumElements())`. Note that if `dest.size() !=
// sharder.NumElements()`, `dest` will be resized.
void SetZero(const Sharder& sharder, Eigen::VectorXd& dest);

// Like `VectorXd::Zero(sharder.NumElements())`.
Eigen::VectorXd ZeroVector(const Sharder& sharder);

// Like `VectorXd::Ones(sharder.NumElements())`.
Eigen::VectorXd OnesVector(const Sharder& sharder);

// Like `dest += scale * increment`.
void AddScaledVector(double scale, const Eigen::VectorXd& increment,
                     const Sharder& sharder, Eigen::VectorXd& dest);

// Like `dest = vec`. `dest` is resized if needed.
void AssignVector(const Eigen::VectorXd& vec, const Sharder& sharder,
                  Eigen::VectorXd& dest);

// Returns a copy of `vec`.
Eigen::VectorXd CloneVector(const Eigen::VectorXd& vec, const Sharder& sharder);

// Like `dest = dest.cwiseProduct(scale)`.
void CoefficientWiseProductInPlace(const Eigen::VectorXd& scale,
                                   const Sharder& sharder,
                                   Eigen::VectorXd& dest);

// Like `dest = dest.cwiseQuotient(scale)`.
void CoefficientWiseQuotientInPlace(const Eigen::VectorXd& scale,
                                    const Sharder& sharder,
                                    Eigen::VectorXd& dest);

// Like `v1.dot(v2)`.
double Dot(const Eigen::VectorXd& v1, const Eigen::VectorXd& v2,
           const Sharder& sharder);
// Like `vector.lpNorm<Eigen::Infinity>()`, a.k.a. LInf norm.
double LInfNorm(const Eigen::VectorXd& vector, const Sharder& sharder);
// Like `vector.lpNorm<1>()`, a.k.a. L_1 norm.
double L1Norm(const Eigen::VectorXd& vector, const Sharder& sharder);
// Like `vector.squaredNorm()`.
double SquaredNorm(const Eigen::VectorXd& vector, const Sharder& sharder);
// Like `vector.norm()`.
double Norm(const Eigen::VectorXd& vector, const Sharder& sharder);

// Like `(vector1 - vector2).squaredNorm()`.
double SquaredDistance(const Eigen::VectorXd& vector1,
                       const Eigen::VectorXd& vector2, const Sharder& sharder);
// Like `(vector1 - vector2).norm()`.
double Distance(const Eigen::VectorXd& vector1, const Eigen::VectorXd& vector2,
                const Sharder& sharder);

// `ScaledL1Norm` is omitted because it's not needed (yet).

// LInf norm of a rescaled vector, i.e.,
// `vector.cwiseProduct(scale).lpNorm<Eigen::Infinity>()`.
double ScaledLInfNorm(const Eigen::VectorXd& vector,
                      const Eigen::VectorXd& scale, const Sharder& sharder);
// Squared L2 norm of a rescaled vector, i.e.,
// `vector.cwiseProduct(scale).squaredNorm()`.
double ScaledSquaredNorm(const Eigen::VectorXd& vector,
                         const Eigen::VectorXd& scale, const Sharder& sharder);
// L2 norm of a rescaled vector, i.e., `vector.cwiseProduct(scale).norm()`.
double ScaledNorm(const Eigen::VectorXd& vector, const Eigen::VectorXd& scale,
                  const Sharder& sharder);

////////////////////////////////////////////////////////////////////////////////
// The functions below compute norms of the columns of a scaled matrix. The
// (i,j) entry of the scaled matrix equals `matrix[i,j] * row_scaling_vec[i]
// * col_scaling_vec[j]`. To ensure good parallelization `matrix` should have
// (roughly) the same location of non-zeros as the `matrix` used to construct
// `sharder`. The size of `sharder` must match the number of columns in
// `matrix`.
////////////////////////////////////////////////////////////////////////////////

// Computes the LInf norm of each column of a scaled `matrix`.
Eigen::VectorXd ScaledColLInfNorm(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& col_scaling_vec, const Sharder& sharder);
// Computes the L2 norm of each column of a scaled `matrix`.
Eigen::VectorXd ScaledColL2Norm(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& col_scaling_vec, const Sharder& sharder);

}  // namespace operations_research::pdlp

#endif  // PDLP_SHARDER_H_
