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

#include "ortools/base/bzip2file.h"

#include <cstdlib>
#include <cstring>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "bzlib.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/file.h"

namespace {

void* OrtBzAlloc(void* opaque, int items, int size) {
  return std::malloc(items * size);
}

void OrtBzFree(void* opaque, void* address) { std::free(address); }

class BZip2InputFile : public File {
 public:
  BZip2InputFile(absl::string_view name, File* file, bool own_file)
      : File(name),
        file_(file),
        own_file_(own_file),
        init_ok_(false),
        stream_completed_(false) {
    memset(&bzstream_, 0, sizeof(bzstream_));
    bzstream_.bzalloc = OrtBzAlloc;
    bzstream_.bzfree = OrtBzFree;
    int bz_status = BZ2_bzDecompressInit(&bzstream_, 0, 0);
    if (bz_status == BZ_OK) {
      init_ok_ = true;
    } else {
      status_ = absl::InternalError("Failed to initialize bzip2 decompressor");
    }
  }

  ~BZip2InputFile() override {
    if (init_ok_) {
      BZ2_bzDecompressEnd(&bzstream_);
    }
    if (own_file_ && file_ != nullptr) {
      file_->Close(0).IgnoreError();
    }
  }

  size_t Read(void* buf, size_t size) override {
    if (!status_.ok() || size == 0) {
      return 0;
    }

    char* dest = static_cast<char*>(buf);
    bzstream_.next_out = dest;
    bzstream_.avail_out = size;

    while (bzstream_.avail_out > 0) {
      if (bzstream_.avail_in == 0) {
        size_t bytes_read = file_->Read(input_buffer_, sizeof(input_buffer_));
        if (bytes_read == 0) {
          if (!stream_completed_) {
            status_ =
                absl::InternalError("Unexpected EOF in bzip2 compressed file");
          }
          break;
        }
        bzstream_.next_in = input_buffer_;
        bzstream_.avail_in = bytes_read;
      }

      if (stream_completed_) {
        char* saved_next_in = bzstream_.next_in;
        unsigned int saved_avail_in = bzstream_.avail_in;
        char* saved_next_out = bzstream_.next_out;
        unsigned int saved_avail_out = bzstream_.avail_out;

        BZ2_bzDecompressEnd(&bzstream_);
        memset(&bzstream_, 0, sizeof(bzstream_));
        bzstream_.bzalloc = OrtBzAlloc;
        bzstream_.bzfree = OrtBzFree;
        bzstream_.next_in = saved_next_in;
        bzstream_.avail_in = saved_avail_in;
        bzstream_.next_out = saved_next_out;
        bzstream_.avail_out = saved_avail_out;

        int init_status = BZ2_bzDecompressInit(&bzstream_, 0, 0);
        if (init_status != BZ_OK) {
          status_ =
              absl::InternalError("Failed to re-initialize bzip2 decompressor");
          break;
        }
        stream_completed_ = false;
      }

      int bz_status = BZ2_bzDecompress(&bzstream_);
      if (bz_status == BZ_STREAM_END) {
        stream_completed_ = true;
      } else if (bz_status != BZ_OK) {
        status_ = absl::InternalError("Bzip2 decompression error");
        break;
      }
    }

    return size - bzstream_.avail_out;
  }

  size_t Write(const void* buf, size_t size) override {
    LOG(FATAL) << "Write not supported on BZip2FileReader";
    return 0;
  }

  absl::Status Close(int flags) override {
    absl::Status status = status_;
    if (init_ok_) {
      BZ2_bzDecompressEnd(&bzstream_);
      init_ok_ = false;
    }
    if (file_ != nullptr) {
      if (own_file_) {
        status.Update(file_->Close(flags));
      }
      file_ = nullptr;
    }
    delete this;
    return status;
  }

  bool Flush() override { return false; }

  size_t Size() override { return 0; }

  bool Open() const override { return file_ != nullptr; }

 private:
  File* file_;
  const bool own_file_;
  bool init_ok_;
  bool stream_completed_;
  absl::Status status_;
  bz_stream bzstream_;
  char input_buffer_[65536];
};

}  // namespace

File* BZip2FileReader(absl::string_view name, File* file, Ownership ownership) {
  return new BZip2InputFile(name, file, ownership == TAKE_OWNERSHIP);
}
