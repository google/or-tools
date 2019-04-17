// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_MODEL_H_
#define OR_TOOLS_SAT_MODEL_H_

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/typeid.h"

namespace operations_research {
namespace sat {

// Class that owns everything related to a particular optimization model.
//
// This class is actually a fully generic wrapper that can hold any type of
// constraints, watchers, solvers and provide a mecanism to wire them together.
class Model {
 public:
  Model() {}

  // This allows to have a nicer API on the client side, and it allows both of
  // these forms:
  //   - ConstraintCreationFunction(contraint_args, &model);
  //   - model.Add(ConstraintCreationFunction(contraint_args));
  //
  // The second form is a bit nicer for the client and it also allows to store
  // constraints and add them later. However, the function creating the
  // constraint is slighly more involved:
  //
  // std::function<void(Model*)> ConstraintCreationFunction(contraint_args) {
  //   return [=] (Model* model) {
  //      ... the same code ...
  //   };
  // }
  //
  // We also have a templated return value for the functions that need it like
  // const BooleanVariable b = model.Add(NewBooleanVariable());
  // const IntegerVariable i = model.Add(NewWeightedSum(weigths, variables));
  template <typename T>
  T Add(std::function<T(Model*)> f) {
    return f(this);
  }

  // Similar to Add() but this is const.
  template <typename T>
  T Get(std::function<T(const Model&)> f) const {
    return f(*this);
  }

  // Returns an object of type T that is unique to this model (like a "local"
  // singleton). This returns an already created instance or create a new one if
  // needed using the T(Model* model) constructor if it exist or T() otherwise.
  //
  // This works a bit like in a dependency injection framework and allows to
  // really easily wire all the classes that make up a solver together. For
  // instance a constraint can depends on the LiteralTrail, or the IntegerTrail
  // or both, it can depend on a Watcher class to register itself in order to
  // be called when needed and so on.
  //
  // IMPORTANT: the Model* constructors function shouldn't form a cycle between
  // each other, otherwise this will crash the program.
  template <typename T>
  T* GetOrCreate() {
    const size_t type_id = gtl::FastTypeId<T>();
    if (!gtl::ContainsKey(singletons_, type_id)) {
      // TODO(user): directly store std::unique_ptr<> in singletons_?
      T* new_t = MyNew<T>(0);
      singletons_[type_id] = new_t;
      TakeOwnership(new_t);
      return new_t;
    }
    return static_cast<T*>(gtl::FindOrDie(singletons_, type_id));
  }

  // Likes GetOrCreate() but do not create the object if it is non-existing.
  // This returns a const version of the object.
  template <typename T>
  const T* Get() const {
    return static_cast<const T*>(
        gtl::FindWithDefault(singletons_, gtl::FastTypeId<T>(), nullptr));
  }

  // Same as Get(), but returns a mutable version of the object.
  template <typename T>
  T* Mutable() const {
    return static_cast<T*>(
        gtl::FindWithDefault(singletons_, gtl::FastTypeId<T>(), nullptr));
  }

  // Gives ownership of a pointer to this model.
  // It will be destroyed when the model is.
  template <typename T>
  void TakeOwnership(T* t) {
    cleanup_list_.emplace_back(new Delete<T>(t));
  }

  // This returns a non-singleton object owned by the model and created with the
  // T(Model* model) constructor if it exist or the T() constructor otherwise.
  // It is just a shortcut to new + TakeOwnership().
  template <typename T>
  T* Create() {
    T* new_t = MyNew<T>(0);
    TakeOwnership(new_t);
    return new_t;
  }

  // Register a non-owned class that will be "singleton" in the model.
  // It is an error to call this on an already registered class.
  template <typename T>
  void Register(T* non_owned_class) {
    const size_t type_id = gtl::FastTypeId<T>();
    CHECK(!gtl::ContainsKey(singletons_, type_id));
    singletons_[type_id] = non_owned_class;
  }

 private:
  // We want to call the constructor T(model*) if it exists or just T() if
  // it doesn't. For this we use some template "magic":
  // - The first MyNew() will only be defined if the type in decltype() exist.
  // - The second MyNew() will always be defined, but because of the ellipsis
  //   it has lower priority that the first one.
  template <typename T>
  decltype(T(static_cast<Model*>(nullptr)))* MyNew(int) {
    return new T(this);
  }
  template <typename T>
  T* MyNew(...) {
    return new T();
  }

  // Map of FastTypeId<T> to a "singleton" of type T.
  std::map</*typeid*/ size_t, void*> singletons_;

  struct DeleteInterface {
    virtual ~DeleteInterface() = default;
  };
  template <typename T>
  class Delete : public DeleteInterface {
   public:
    explicit Delete(T* t) : to_delete_(t) {}
    ~Delete() override = default;

   private:
    std::unique_ptr<T> to_delete_;
  };

  // The list of items to delete.
  //
  // TODO(user): I don't think we need the two layers of unique_ptr, but we
  // don't care too much about efficiency here and this was easier to get
  // working.
  std::vector<std::unique_ptr<DeleteInterface>> cleanup_list_;

  DISALLOW_COPY_AND_ASSIGN(Model);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_MODEL_H_
