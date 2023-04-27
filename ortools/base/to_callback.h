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

// ToCallback allows a std::function<> object to be converted to
// Closure/Callback types accepted by older APIs. In fact, any functor, i.e.,
// a pure function or an object that has an "operator()", can be converted to
// Closure/Callback.
//
// Background: C++11 provides a std::function<Result(Args...)> type that stands
// for a typed piece of code that can be invoked. This is similar to the older
// functionality of Closure/Callback in callback.h.

#ifndef OR_TOOLS_BASE_TO_CALLBACK_H_
#define OR_TOOLS_BASE_TO_CALLBACK_H_

#include <type_traits>

#include "absl/log/check.h"
#include "ortools/base/logging.h"

template <typename C>
class CallbackToSig {
  template <typename F>
  struct MFSig;
  template <typename R, typename U, typename... A>
  struct MFSig<R (U::*)(A...)> {
    using type = R(A...);
  };

 public:
  using type = typename MFSig<decltype(&C::Run)>::type;
};

template <typename CallbackType, typename Functor,
          typename Sig = typename CallbackToSig<CallbackType>::type>
class FunctorCallback;

template <typename CallbackType, typename Functor, typename R, typename... Args>
class FunctorCallback<CallbackType, Functor, R(Args...)> final
    : public CallbackType {
 public:
  explicit FunctorCallback(Functor&& functor)
      : CallbackType(), functor_(std::move(functor)) {}

  R Run(Args... args) override {
    RunCallbackHelper helper(this);
    return static_cast<R>(
        static_cast<Functor&&>(functor_)(std::forward<Args>(args)...));
  }

 private:
  struct RunCallbackHelper {
    explicit RunCallbackHelper(FunctorCallback* cb) : cb(cb) {}
    ~RunCallbackHelper() { delete cb; }

    FunctorCallback* cb;
  };

  Functor functor_;
};

template <typename Functor>
class FunctorCallbackBinder {
 public:
  explicit FunctorCallbackBinder(const Functor& functor) : functor_(functor) {}

  template <typename CallbackType>
  operator CallbackType*() && {  // NOLINT
    CHECK(!bound_) << "Returned ToCallback object has already been converted";
    bound_ = true;
    return new FunctorCallback<CallbackType, Functor>(std::move(functor_));
  }

 private:
  Functor functor_;
  bool bound_ = false;
};

template <typename Functor>
using ToCallbackResult =
    FunctorCallbackBinder<typename std::decay<Functor>::type>;

template <typename Functor>
ToCallbackResult<Functor> ToCallback(Functor&& functor) {
  return ToCallbackResult<Functor>(std::forward<Functor>(functor));
}

#endif  // OR_TOOLS_BASE_TO_CALLBACK_H_
