// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_BASE_UNIQUE_ARRAY_H_
#define ORTOOLS_BASE_UNIQUE_ARRAY_H_

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"

namespace gtl {

template <typename T, typename Deleter = void>
class ABSL_ATTRIBUTE_TRIVIAL_ABI ABSL_NULLABILITY_COMPATIBLE UniqueArray;

template <typename T>
UniqueArray<T> MakeUniqueArray(size_t size);

template <typename T>
UniqueArray<T> MakeUniqueArrayForOverwrite(size_t size);

namespace internal {

// This crashing logic does not need to be optimized for speed, so we factor it
// out of the class template into a free function in the .cc file to avoid
// bloating executables' object code with many copies of it.
//
// REQUIRES: size != 0.
[[noreturn]] void CrashOnNullUniqueArrayWithNonzeroSize(size_t size);

}  // namespace internal

// `UniqueArray<T>` is a replacement for `std::unique_ptr<T[]>` that keeps track
// of its size. It is intended as a low-level type to migrate towards the Safe
// Buffers Programming Model. Its features include:
// * Bound-checked indexing via hardened `operator[]`.
// * Supports construction of spans with `absl::MakeSpan()`.
// * Supports incremental adoption via `release()`, allowing the user to get
// ownership of the underlying `std::unique_ptr<T[]>`.
// * Supports validating a user-provided size when constructing a `UniqueArray`
// from an existing pointer (either raw or smart). The validation uses cookies
// as defined by the C++ Itanium ABI for `new[]` allocations, and is therefore
// limited to the cases where the array cookie is available.
//
// `UniqueArray` has a well-defined moved-from state, identical to the
// default-constructed state.
template <typename T, typename Deleter>
class ABSL_ATTRIBUTE_TRIVIAL_ABI ABSL_NULLABILITY_COMPATIBLE UniqueArray {
 public:
  static_assert(!std::is_reference_v<T>,
                "UniqueArray cannot hold reference types");

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using deleter_type = std::conditional_t<std::is_void_v<Deleter>,
                                          std::default_delete<T[]>, Deleter>;

  // This type wraps a unique_ptr `ptr` and its `size`. It is used when
  // releasing ownership of the unique_ptr to callers of `release()`, without
  // losing the size information.
  struct [[nodiscard]] OwningPointer {
    std::unique_ptr<T[], deleter_type> ptr;
    size_t size;
  };

  // Constructs an empty `UniqueArray` and does not allocate any memory.
  UniqueArray() = default;

  // Constructs an `UniqueArray` with a default-constructed state.
  // NOLINTBEGIN(google-explicit-constructor) - Allow implicit construction
  // from nullptr.
  UniqueArray(std::nullptr_t) : UniqueArray() {}
  // NOLINTEND(google-explicit-constructor)

  // Constructs a `UniqueArray` who takes ownership of the allocation pointed by
  // the raw pointer `ptr`. The pointer `ptr` points to an array big enough
  // to store `size` elements of type `T`, that must be possible to destroy and
  // deallocate through a deleter of type `deleter_type` which defaults to the
  // `delete[]` operation. The `deleter_type` must be default constructible,
  // otherwise (Ex: lambdas) the constructor taking an existing unique_ptr
  // should be used.
  //
  // When `ptr` is nullptr:
  // - `size` must be 0. The requirement is enforced and the constructor fails
  // when not met.
  //
  // When `ptr` is not nullptr:
  // - `ptr` must be correctly aligned for type `T`, otherwise undefined
  // behavior may happen. The requirement is not enforced because
  // `std::unique_ptr` doesn't either, and we don't want to enforce new
  // requirements beyond size.
  // - `ptr` must point to an array of `size` or more elements. The requirement
  // is enforced, and the constructor fails,  when an array cookie is available
  // according to the C++ Itanium ABI, and Abseil hardening is enabled.
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::unsafe_buffer_usage)
  [[clang::unsafe_buffer_usage]]
#endif
  UniqueArray(T* ptr, size_t size)
    requires std::is_default_constructible_v<deleter_type>
      : data_(std::unique_ptr<T[], deleter_type>(ptr)), size_(size) {
    if (ptr == nullptr) {
      if (size != 0) internal::CrashOnNullUniqueArrayWithNonzeroSize(size);
    }
  }

  // Constructs a `UniqueArray` from an existing `std::unique_ptr<T[]>` `ptr` of
  // `size` or more elements, that must be possible to destroy and deallocate
  // through a deleter of type `deleter_type`, which defaults to the
  // `delete[]` operation.
  //
  // When `ptr` is nullptr:
  // - `size` must be 0. The requirement is enforced and the constructor fails
  // when not met.
  //
  // When `ptr` is not nullptr:
  // - `ptr` must point to an array of `size` or more elements. The requirement
  // is enforced, and the constructor fails, when an array cookie is available
  // according to the C++ Itanium ABI, and Abseil hardening is enabled.
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::unsafe_buffer_usage)
  [[clang::unsafe_buffer_usage]]
#endif
  UniqueArray(std::unique_ptr<T[], deleter_type> ptr, size_t size)
      : data_(std::move(ptr)), size_(size) {
    if (data_ == nullptr) {
      if (size != 0) internal::CrashOnNullUniqueArrayWithNonzeroSize(size);
    }
  }

  // Equivalent to `UniqueArray(il.begin(), il.end())`.
  UniqueArray(std::initializer_list<T> il)
      : UniqueArray(il.begin(), il.end()) {}

  // Constructs a `UniqueArray` with the contents of the range [first, last).
  // Requires a forward iterator because we need to know the distance before
  // iterating for the copy.
  template <typename Iterator>
  UniqueArray(Iterator first ABSL_ATTRIBUTE_LIFETIME_BOUND,
              Iterator last ABSL_ATTRIBUTE_LIFETIME_BOUND)
    requires(std::forward_iterator<Iterator>)
  {
    size_ = std::distance(first, last);
    data_ = std::unique_ptr<T[], deleter_type>(new T[size_]);
    std::copy(first, last, data());
  }

  // Move-only type since the memory is owned.
  UniqueArray(const UniqueArray&) = delete;
  UniqueArray& operator=(const UniqueArray&) = delete;

  // Move-construction leaves the moved-from object in a default-constructed
  // state.
  UniqueArray(UniqueArray&& that) noexcept
      : data_(std::move(that.data_)),
        size_(std::exchange(that.size_, std::size_t{0})) {}

  // Converting construction leaves the moved-from object in a
  // default-constructed state.
  //
  // The type template parameters `T1` and `D1` make reference to the
  // `UniqueArray` being assigned, and `UniqueArray<T1, D1>::pointer` must be
  // implicitly convertible to `pointer`.
  // NOLINTBEGIN(google-explicit-constructor) - For compatibility with
  // unique_ptr.
  template <typename T1, typename D1>
  UniqueArray(UniqueArray<T1, D1>&& that) {
    size_ = that.size();
    data_ = std::move(that.release().ptr);
  }
  // NOLINTEND(google-explicit-constructor)

  // Move-assignment leaves the moved-from object in a default-constructed
  // state. Returns `*this`.
  UniqueArray& operator=(UniqueArray&& that) noexcept {
    data_ = std::move(that.data_);
    size_ = std::exchange(that.size_, std::size_t{0});
    return *this;
  }

  // Converting assignment leaves the moved-from object in a default-constructed
  // state. Returns `*this`.
  //
  // The type template parameters `T1` and `D1` make reference to the
  // `UniqueArray` being assigned, and `UniqueArray<T1, D1>::pointer` must be
  // implicitly convertible to `pointer`.
  template <typename T1, typename D1>
  UniqueArray& operator=(UniqueArray<T1, D1>&& that) {
    size_ = that.size();
    data_ = std::move(that.release().ptr);
    return *this;
  }

  // Assignment from nullptr resets the managed unique_ptr and leaves `*this`
  // in the default-constructed state. Returns `*this`.
  UniqueArray& operator=(std::nullptr_t) {
    data_.reset();
    size_ = std::size_t{0};
    return *this;
  }

#ifdef __cpp_sized_deallocation
  ~UniqueArray() {
    if constexpr (std::is_void_v<Deleter> && !has_array_cookie<T>::value) {
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
      if (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        ::operator delete[]((void*)data_.release(), sizeof(T) * size_,
                            std::align_val_t{alignof(T)});
        return;
      }
#endif  // __STDCPP_DEFAULT_NEW_ALIGNMENT__
      ::operator delete[]((void*)data_.release(), sizeof(T) * size_);
    }
  }
#else
  ~UniqueArray() = default;
#endif

  // Returns `true` when `*this` owns an allocation, `false` otherwise.
  explicit operator bool() const { return data_ != nullptr; }

  // Returns a raw pointer to the underlying contiguous storage.
  T* data() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return data_.get(); }

  // Returns a reference to the element in position `index`. Fails when `index`
  // is out of bounds and Abseil hardening is enabled.
  T& operator[](size_t idx) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_HARDENING_ASSERT(idx < size());
    return data_[idx];
  }

  // Returns the number of the elements in the owned allocation.
  size_t size() const { return size_; }

  // Releases ownership of the managed unique_ptr, returning the unique_ptr and
  // the size of the array, leaving the `UniqueArray` in a moved-from state.
  OwningPointer release() {
    return {std::move(data_), std::exchange(size_, 0)};
  }

  // Replaces the managed array with a new `UniqueArray` created from `ptr` and
  // `size`. For `ptr` and `size`, the same requirements of the constructor
  // `UniqueArray(T* ptr, size_t size)` apply.
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::unsafe_buffer_usage)
  [[clang::unsafe_buffer_usage]]
#endif
  void reset(T* ptr, size_t size) {
    *this = UniqueArray(ptr, size);
  }

  // Resets the managed unique_ptr and leaves `*this` in the default-constructed
  // state.
  void reset(std::nullptr_t = nullptr) {
    data_.reset();
    size_ = std::size_t{0};
  }

  // Enables `AbslStringify` to provide logging compatibility with
  // `std::unique_ptr` pointers.
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const UniqueArray& uniq_arr) {
    absl::Format(&sink, "%p", uniq_arr.data());
  }

  // Enables stream `operator<<"` to provide printing compatibility with
  // `std::unique_ptr` pointers.
  friend std::ostream& operator<<(std::ostream& os,
                                  const UniqueArray& uniq_arr) {
    return os << uniq_arr.data();
  }

 private:
  // Replacement for  non-standard API `std::__is_default_deleter`. Returns
  // `std::true_type` when the default destructor for `std::unique_ptr` is in
  // use, `std::false_type` otherwise.
  template <typename ArrayDeleter>
  struct is_default_deleter : std::false_type {};

  template <typename ArrayType>
  struct is_default_deleter<std::default_delete<ArrayType> > : std::true_type {
  };

  // Replacement for non-standard API `std::__has_array_cookie`. Returns
  // `true_type` when the array type requires an array cookie according to the
  // Itanium C++ ABI, `false_type` otherwise.
  template <typename ArrayType>
  struct has_array_cookie
      : std::negation<std::is_trivially_destructible<ArrayType> > {};

  // Constructs a `UniqueArray` from an existing `std::unique_ptr<T[]>` `ptr` of
  // `size` elements.
  // Uses tag dispatching to distinguish itself of the public version of the
  // constructor. The private version is to be used by `MakeUniqueArray` and
  // `MakeUninitializedUniqueArray` factory functions only. Hence, the `ptr` and
  // `size` arguments are trusted, and requirements/hardening are not enforced.
  UniqueArray(std::unique_ptr<T[], deleter_type> ptr, size_t size,
              std::true_type trusted)
      : data_(std::move(ptr)), size_(size) {}

  // The factory methods `MakeUniqueArray` and `MakeUniqueArrayUninitialized`
  // are marked as friends so they can use the private `UniqueArray` constructor
  // and skip checks/hardening on the pointer and size provided.
  friend UniqueArray<T> MakeUniqueArray<>(size_t size);
  friend UniqueArray<T> MakeUniqueArrayForOverwrite<>(size_t size);

  // Replacement for the non-standard API `std::__get_array_cookie`. `ptr` is a
  // pointer to a `new[]` allocation expected to have an array cookie according
  // to the Itanium C++ ABI.
  template <typename ArrayType>
  // Using the same attribute than libc++ to avoid failures when
  // -fsanitize-address-poison-custom-array-cookie is enabled.
  ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS size_t
  get_array_cookie(ArrayType const* ptr) {
    static_assert(has_array_cookie<ArrayType>::value,
                  "Trying to access the array cookie of a type that is not "
                  "guaranteed to have one");
    size_t const* cookie = reinterpret_cast<size_t const*>(ptr) - 1;
    return *cookie;
  }

  // When `ptr` points to a `new[]` allocation, storing a cookie with the
  // allocated length, as described in the Itanium C++ ABI, returns `true` when
  // the `size` is smaller or equal than the cookie, `false` otherwise. In cases
  // where there is no cookie, the provided `size` can not be validated and we
  // have to trust it, hence we always return `true`.
  //
  // In debug builds, when there is no `new[]` cookie available, the user
  // provided size is validated using the allocation size recorded by tcmalloc.
  template <class ArrayDeleter, class ArrayType>
  constexpr bool size_le_than_cookie(ArrayType const* ptr, size_t size) {
    if constexpr (is_default_deleter<ArrayDeleter>::value) {
      if constexpr (has_array_cookie<ArrayType>::value) {
        size_t cookie = get_array_cookie(ptr);
        return size <= cookie;
      }
    }

    return true;
  }

  std::unique_ptr<T[], deleter_type> data_;
  size_t size_ = 0u;
};

// Allocates memory capable of holding `size` elements and performs value
// initialization for each element (matching the behavior of
// `std::make_unique`).
template <typename T>
UniqueArray<T> MakeUniqueArray(size_t size) {
  static_assert(std::constructible_from<T>);
  return UniqueArray<T>(std::unique_ptr<T[]>(new T[size]()), size,
                        std::true_type{});
}

// Allocates memory capable of holding `size` elements and performs default
// initialization for each element (matching the behavior of
// `std::make_unique_for_overwrite`).
template <typename T>
UniqueArray<T> MakeUniqueArrayForOverwrite(size_t size) {
  static_assert(std::constructible_from<T>);
  return UniqueArray<T>(std::unique_ptr<T[]>(new T[size]), size,
                        std::true_type{});
}

// Equality operator with a null pointer. Returns `true` when `buffer` does not
// not own an allocation, `false` otherwise.
template <typename T, typename D>
inline bool operator==(const UniqueArray<T, D>& buffer, std::nullptr_t) {
  return !buffer;
}

template <typename T, typename D>
inline bool operator==(std::nullptr_t, const UniqueArray<T, D>& buffer) {
  return !buffer;
}

// Inequality operator with a null pointer. Returns `true` when `buffer` owns an
// allocation, `false` otherwise.
template <typename T, typename D>
inline bool operator!=(const UniqueArray<T, D>& buffer, std::nullptr_t) {
  return static_cast<bool>(buffer);
}

template <typename T, typename D>
inline bool operator!=(std::nullptr_t, const UniqueArray<T, D>& buffer) {
  return static_cast<bool>(buffer);
}

}  // namespace gtl

#endif  // ORTOOLS_BASE_UNIQUE_ARRAY_H_
