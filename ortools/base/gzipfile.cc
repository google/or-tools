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

#include "ortools/base/gzipfile.h"

#include <zconf.h>
#include <zlib.h>

#include <cstring>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/file.h"

namespace {

class GZipInputFile : public File {
 public:
  GZipInputFile(absl::string_view name, File* file, bool own_file)
      : File(name),
        file_(file),
        own_file_(own_file),
        init_ok_(false),
        stream_completed_(false) {
    memset(&zstream_, 0, sizeof(zstream_));
    int z_status = inflateInit2(&zstream_, 47);
    if (z_status == Z_OK) {
      init_ok_ = true;
    } else {
      status_ = absl::InternalError("Failed to initialize zlib decompressor");
    }
  }

  ~GZipInputFile() override {
    if (init_ok_) {
      inflateEnd(&zstream_);
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
    zstream_.next_out = reinterpret_cast<Bytef*>(dest);
    zstream_.avail_out = size;

    while (zstream_.avail_out > 0) {
      if (zstream_.avail_in == 0) {
        size_t bytes_read = file_->Read(input_buffer_, sizeof(input_buffer_));
        if (bytes_read == 0) {
          if (!stream_completed_) {
            status_ =
                absl::InternalError("Unexpected EOF in gzip compressed file");
          }
          break;
        }
        zstream_.next_in = reinterpret_cast<Bytef*>(input_buffer_);
        zstream_.avail_in = bytes_read;
      }

      if (stream_completed_) {
        Bytef* saved_next_in = zstream_.next_in;
        unsigned int saved_avail_in = zstream_.avail_in;
        Bytef* saved_next_out = zstream_.next_out;
        unsigned int saved_avail_out = zstream_.avail_out;

        inflateEnd(&zstream_);
        memset(&zstream_, 0, sizeof(zstream_));
        zstream_.next_in = saved_next_in;
        zstream_.avail_in = saved_avail_in;
        zstream_.next_out = saved_next_out;
        zstream_.avail_out = saved_avail_out;

        int init_status = inflateInit2(&zstream_, 47);
        if (init_status != Z_OK) {
          status_ =
              absl::InternalError("Failed to re-initialize zlib decompressor");
          break;
        }
        stream_completed_ = false;
      }

      int z_status = inflate(&zstream_, Z_NO_FLUSH);
      if (z_status == Z_STREAM_END) {
        stream_completed_ = true;
      } else if (z_status != Z_OK && z_status != Z_BUF_ERROR) {
        status_ = absl::InternalError("Zlib decompression error");
        break;
      }
    }

    return size - zstream_.avail_out;
  }

  size_t Write(const void* buf, size_t size) override {
    LOG(FATAL) << "Write not supported on GZipFileReader";
    return 0;
  }

  absl::Status Close(int flags) override {
    absl::Status status = status_;
    if (init_ok_) {
      inflateEnd(&zstream_);
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
  z_stream zstream_;
  char input_buffer_[65536];
};

}  // namespace

File* GZipFileReader(absl::string_view name, File* file, Ownership ownership) {
  return new GZipInputFile(name, file, ownership == TAKE_OWNERSHIP);
}
