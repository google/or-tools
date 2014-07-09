// Copyright 2010-2014 Google
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

// An all-in one inclusion (for simplicity) of the {dense,sparse}_hash_{map,set}
// files in the Google sparsehash open-source library.

#ifndef OR_TOOLS_BASE_SPARSE_HASH_H_
#define OR_TOOLS_BASE_SPARSE_HASH_H_

#include <google/dense_hash_map>
#include <google/dense_hash_set>
#include <google/sparse_hash_map>
#include <google/sparse_hash_set>

using google::dense_hash_map;
using google::dense_hash_set;
using google::sparse_hash_map;
using google::sparse_hash_set;

#endif  // OR_TOOLS_BASE_SPARSE_HASH_H_
