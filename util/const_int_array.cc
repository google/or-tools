// Copyright 2010-2011 Google
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

#include <string.h>
#include <algorithm>
#include <functional>
#include "base/stringprintf.h"
#include "util/const_int_array.h"
#include "util/bitset.h"

namespace operations_research {
ConstIntArray::ConstIntArray(int64 v0)
    : data_(new vector<int64>(1)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
}

ConstIntArray::ConstIntArray(int64 v0, int64 v1)
    : data_(new vector<int64>(2)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
  (*data_)[1] = v1;
}

ConstIntArray::ConstIntArray(int64 v0, int64 v1, int64 v2)
    : data_(new vector<int64>(3)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
  (*data_)[1] = v1;
  (*data_)[2] = v2;
}

ConstIntArray::ConstIntArray(int64 v0, int64 v1, int64 v2, int64 v3)
    : data_(new vector<int64>(4)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
  (*data_)[1] = v1;
  (*data_)[2] = v2;
  (*data_)[3] = v3;
}

ConstIntArray::ConstIntArray(int64 v0, int64 v1, int64 v2, int64 v3, int64 v4)
    : data_(new vector<int64>(5)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
  (*data_)[1] = v1;
  (*data_)[2] = v2;
  (*data_)[3] = v3;
  (*data_)[4] = v4;
}

ConstIntArray::ConstIntArray(int64 v0, int64 v1, int64 v2, int64 v3, int64 v4,
                             int64 v5)
    : data_(new vector<int64>(6)), scanned_(false), status_(0) {
  (*data_)[0] = v0;
  (*data_)[1] = v1;
  (*data_)[2] = v2;
  (*data_)[3] = v3;
  (*data_)[4] = v4;
  (*data_)[5] = v5;
}

ConstIntArray::ConstIntArray(const vector<int64>& data)
    : data_(new vector<int64>(data)),
      scanned_(false),
      status_(0) {}

ConstIntArray::ConstIntArray(const vector<int>& data)
    : data_(new vector<int64>(data.size())),
      scanned_(false),
      status_(0) {
  for (int i = 0; i < data.size(); ++i) {
    (*data_)[i] = data[i];
  }
}

ConstIntArray::ConstIntArray(const int64 * const data, int size)
    : data_(new vector<int64>(size)), scanned_(false), status_(0)  {
  CHECK_GT(size, 0);
  CHECK_NOTNULL(data);
  memcpy((data_->data()), data, size * sizeof(*data));
}

ConstIntArray::ConstIntArray(const int * const data, int size)
    : data_(new vector<int64>(size)), scanned_(false), status_(0) {
  CHECK_GT(size, 0);
  CHECK_NOTNULL(data);
  for (int i = 0; i < size; ++i) {
    (*data_)[i] = data[i];
  }
}

ConstIntArray::ConstIntArray(vector<int64>* data)
    : data_(data), scanned_(false), status_(0) {
  CHECK_GT(data->size(), 0);
}

ConstIntArray::~ConstIntArray() {}

vector<int64>* ConstIntArray::Release() {
  return data_.release();
}

int ConstIntArray::size() const {
  CHECK_NOTNULL(data_.get());
  return data_->size();
}

vector<int64>* ConstIntArray::SortedCopy(bool increasing) const {
  vector<int64>* new_data = new vector<int64>(*data_);
  if (increasing) {
    std::sort(new_data->begin(), new_data->end());
  } else {
    std::sort(new_data->begin(), new_data->end(), std::greater<int64>());
  }
  return new_data;
}

vector<int64>*
ConstIntArray::SortedCopyWithoutDuplicates(bool increasing) const {
  vector<int64>* new_data = new vector<int64>(*data_);
  if (increasing) {
    std::sort(new_data->begin(), new_data->end());
  } else {
    std::sort(new_data->begin(), new_data->end(), std::greater<int64>());
  }
  int duplicates = 0;
  for (int i = 0; i < data_->size() - 1; ++i) {
    duplicates += (*new_data)[i] == (*new_data)[i + 1];
  }
  if (duplicates == 0) {
    return new_data;
  } else {
    const int final_size = data_->size() - duplicates;
    vector<int64>* final_data = new vector<int64>(final_size);
    (*final_data)[0] = (*new_data)[0];
    int counter = 1;
    for (int i = 1; i < data_->size(); ++i) {
      if ((*new_data)[i] != (*new_data)[i - 1]) {
        (*final_data)[counter++] = (*new_data)[i];
      }
    }
    CHECK_EQ(final_size, counter);
    delete new_data;
    return final_data;
  }
}

bool ConstIntArray::Equals(const ConstIntArray& other)  {
  if (data_->size() != other.data_->size()) {
    return false;
  }
  for (int i = 0; i < data_->size(); ++i) {
    if ((*data_)[i] != other[i]) {
      return false;
    }
  }
  return true;
}

bool ConstIntArray::HasProperty(Property info) {
  const int kBitsInChar = 8;
  DCHECK_GE(info, 0);
  DCHECK_LT(info, kBitsInChar * sizeof(status_));
  CHECK_NOTNULL(data_.get());
  if (!scanned_) {
    Scan();
  }
  return IsBitSet64(&status_, info);
}

void ConstIntArray::AndProperty(Property info, bool value) {
  if (!value) {
    ClearBit64(&status_, info);
  }
}

void ConstIntArray::Scan() {
  DCHECK(!scanned_);
  status_ = IntervalDown64(NUM_PROPERTY);
  scanned_ = true;
  if (data_.get() == NULL) {  // After a release.
    return;
  }
  const int64 first = (*data_)[0];
  AndProperty(IS_POSITIVE, first > 0);
  AndProperty(IS_NEGATIVE, first < 0);
  AndProperty(IS_NEGATIVE_OR_NULL, first <= 0);
  AndProperty(IS_POSITIVE_OR_NULL, first >= 0);
  AndProperty(IS_BOOLEAN, first == 0 || first == 1);

  for (int i = 1; i < data_->size(); ++i) {
    const int64 previous = (*data_)[i - 1];
    const int64 current = (*data_)[i];
    AndProperty(IS_CONSTANT, current == previous);
    AndProperty(IS_DECREASING, previous >= current);
    AndProperty(IS_STRICTLY_DECREASING, previous > current);
    AndProperty(IS_INCREASING, previous <= current);
    AndProperty(IS_STRICTLY_INCREASING, previous < current);
    AndProperty(IS_BOOLEAN, current == 0 || current == 1);
    AndProperty(IS_POSITIVE, current > 0);
    AndProperty(IS_NEGATIVE, current < 0);
    AndProperty(IS_NEGATIVE_OR_NULL, current <= 0);
    AndProperty(IS_POSITIVE_OR_NULL, current >= 0);
    if (!status_) {
      break;
    }
  }
}

string ConstIntArray::DebugString() const {
  if (data_.get() == NULL) {
    return "Released ConstIntArray";
  }
  string result = "[";
  for (int i = 0; i < data_->size(); ++i) {
    StringAppendF(&result, StringPrintf("%lld", (*data_)[i]).c_str());
    if (i != data_->size() - 1) {
      StringAppendF(&result, ", ");
    }
  }
  StringAppendF(&result, "]");
  return result;
}
}  // namespace operations_research
