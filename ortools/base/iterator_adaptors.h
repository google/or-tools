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

#ifndef OR_TOOLS_BASE_ITERATOR_ADAPTORS_H_
#define OR_TOOLS_BASE_ITERATOR_ADAPTORS_H_

namespace gtl {

template <class Container>
class ReverseView {
 public:
  typedef typename Container::const_reverse_iterator const_it;

  ReverseView(const Container& c) : c_(c) {}
  const_it begin() const { return c_.rbegin(); }
  const_it end() const { return c_.rend(); }

 private:
  const Container& c_;
};

template <class Container>
ReverseView<Container> reversed_view(const Container& c) {
  return ReverseView<Container>(c);
}

}  // namespace gtl

#endif  // OR_TOOLS_BASE_ITERATOR_ADAPTORS_H_
