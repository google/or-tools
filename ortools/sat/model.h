// Copyright 2010-2017 Google
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

  // Returns an object of type T that is unique to this model (this is a bit
  // like a "local" singleton). This returns an already created instance or
  // create a new one if needed using the T(Model* model) constructor.
  //
  // This works a bit like in a dependency injection framework and allows to
  // really easily wire all the classes that make up a solver together. For
  // instance a constraint can depends on the LiteralTrail, or the IntegerTrail
  // or both, it can depend on a Watcher class to register itself in order to
  // be called when needed and so on.
  //
  // IMPORTANT: the Model* constructors function shouldn't form a cycle between
  // each other, otherwise this will crash the program.
  //
  // TODO(user): Rename to GetOrCreateSingleton().
  template <typename T>
  T* GetOrCreate() {
    const size_t type_id = FastTypeId<T>();
    if (!ContainsKey(singletons_, type_id)) {
      // TODO(user): directly store std::unique_ptr<> in singletons_?
      T* new_t = new T(this);
      singletons_[type_id] = new_t;
      TakeOwnership(new_t);
      return new_t;
    }
    return static_cast<T*>(FindOrDie(singletons_, type_id));
  }

  // This returns a non-singleton object owned by the model.
  template <typename T>
  T* Create() {
    T* new_t = new T(this);
    TakeOwnership(new_t);
    return new_t;
  }

  // Registers a given instance of type T as a "local singleton" for this type.
  // For now this CHECKs that the object was not yet created.
  template <typename T>
  void SetSingleton(std::unique_ptr<T> t) {
    const size_t type_id = FastTypeId<T>();
    CHECK(!ContainsKey(singletons_, type_id));
    singletons_[type_id] = t.get();
    TakeOwnership(t.release());
  }

  // Likes GetOrCreate() but do not create the object if it is non-existing.
  // This returns a const version of the object.
  template <typename T>
  const T* Get() const {
    return static_cast<const T*>(
        FindWithDefault(singletons_, FastTypeId<T>(), nullptr));
  }

  // Same as Get(), but returns a mutable version of the object.
  template <typename T>
  T* Mutable() const {
    return static_cast<T*>(
        FindWithDefault(singletons_, FastTypeId<T>(), nullptr));
  }

  // Gives ownership of a pointer to this model.
  // It will be destroyed when the model is.
  template <typename T>
  void TakeOwnership(T* t) {
    cleanup_list_.emplace_back(new Delete<T>(t));
  }

 private:
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
