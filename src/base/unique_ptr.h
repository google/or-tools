// Copyright 2010-2013 Google
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

#ifndef OR_TOOLS_BASE_UNIQUE_PTR_H_
#define OR_TOOLS_BASE_UNIQUE_PTR_H_

#include <memory>

#if defined(__APPLE__) && (__clang_major__ < 5)
#include <assert.h>
#include "base/macros.h"
namespace std {
// A unique_ptr<T> is like a T*, except that the destructor of unique_ptr<T>
// automatically deletes the pointer it holds (if any).
// That is, unique_ptr<T> owns the T object that it points to.
// Like a T*, a unique_ptr<T> may hold either NULL or a pointer to a T object.
//
// The size of a unique_ptr is small:
// sizeof(unique_ptr<C>) == sizeof(C*)
template <class C>
class unique_ptr {
 public:
  // The element type
  typedef C element_type;

  // Constructor.  Defaults to intializing with NULL.
  // There is no way to create an uninitialized unique_ptr.
  // The input parameter must be allocated with new.
  explicit unique_ptr(C* p = NULL) : ptr_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~unique_ptr() {
    enum { type_must_be_complete = sizeof(C) };
    delete ptr_;
  }

  // Reset.  Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(C) };
      delete ptr_;
      ptr_ = p;
    }
  }

  // Accessors to get the owned object.
  // operator* and operator-> will assert() if there is no current object.
  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }
  C* operator->() const  {
    assert(ptr_ != NULL);
    return ptr_;
  }
  C* get() const { return ptr_; }

  // Comparison operators.
  // These return whether two unique_ptr refer to the same object, not just to
  // two different but equal objects.
  bool operator==(C* p) const { return ptr_ == p; }
  bool operator!=(C* p) const { return ptr_ != p; }

  // Swap two scoped pointers.
  void swap(unique_ptr& p2) {
    C* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() {
    C* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

 private:
  C* ptr_;

  // Forbid comparison of unique_ptr types.  If C2 != C, it totally doesn't
  // make sense, and if C2 == C, it still doesn't make sense because you should
  // never have the same object owned by two different unique_ptrs.
  template <class C2> bool operator==(unique_ptr<C2> const& p2) const;
  template <class C2> bool operator!=(unique_ptr<C2> const& p2) const;

  DISALLOW_COPY_AND_ASSIGN(unique_ptr);
};

// Free functions
template <class C>
void swap(unique_ptr<C>& p1, unique_ptr<C>& p2) {
  p1.swap(p2);
}

template <class C>
bool operator==(C* p1, const unique_ptr<C>& p2) {
  return p1 == p2.get();
}

template <class C>
bool operator!=(C* p1, const unique_ptr<C>& p2) {
  return p1 != p2.get();
}

// Specialization of unique_ptr used for holding arrays:
//
// unique_ptr<int[]> array(new int[10]);
//
// This specialization provides operator[] instead of operator* and
// operator->, and by default it deletes the stored array using 'delete[]'
// rather than 'delete'.
template <class C>
class unique_ptr<C[]> {
 public:
  // The element type
  typedef C element_type;

  // Constructor.  Defaults to intializing with NULL.
  // There is no way to create an uninitialized unique_ptr.
  // The input parameter must be allocated with new.
  explicit unique_ptr(C* p = NULL) : ptr_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~unique_ptr() {
    enum { type_must_be_complete = sizeof(C) };
    delete [] ptr_;
  }

  // Reset.  Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(C) };
      delete [] ptr_;
      ptr_ = p;
    }
  }

  // Accessors to get the owned object.
  // operator[] and operator-> will assert() if there is no current object.
  C& operator[] (size_t i) const {
    assert(ptr_ != NULL);
    return ptr_[i];
  }

  C* get() const { return ptr_; }

  // Comparison operators.
  // These return whether two unique_ptr refer to the same object, not just to
  // two different but equal objects.
  bool operator==(C* p) const { return ptr_ == p; }
  bool operator!=(C* p) const { return ptr_ != p; }

  // Swap two scoped pointers.
  void swap(unique_ptr& p2) {
    C* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() {
    C* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

 private:
  C* ptr_;
  DISALLOW_COPY_AND_ASSIGN(unique_ptr);
};
}  // namespace std
#endif  // defined(__APPLE__) && (__clang_major__ < 5)
#endif  // OR_TOOLS_BASE_UNIQUE_PTR_H_
