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

#ifndef OR_TOOLS_BASE_FLAGS_H_
#define OR_TOOLS_BASE_FLAGS_H_

#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// These flags should not be used in C++ code to access logging library
// configuration knobs. Use interfaces defined in absl/log/globals.h
// instead. It is still ok to use these flags on a command line.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// Set whether log messages go to stderr *instead* of logfiles.
ABSL_DECLARE_FLAG(bool, logtostderr);

// Set whether log messages go to stderr in addition to logfiles.
ABSL_DECLARE_FLAG(bool, alsologtostderr);

// Log messages at a level >= this flag are automatically sent to
// stderr in addition to log files.
ABSL_DECLARE_FLAG(int, stderrthreshold);

// Log suppression level: messages logged at a lower level than this
// are suppressed.
ABSL_DECLARE_FLAG(int, minloglevel);

// Log messages at a level <= this flag are buffered.
// Log messages at a higher level are flushed immediately.
ABSL_DECLARE_FLAG(int, logbuflevel);

// Set whether the log prefix should be prepended to each line of output.
ABSL_DECLARE_FLAG(bool, log_prefix);

// Global lob verbosity level. Default in vlog_is_on.cc
ABSL_DECLARE_FLAG(int, v);

// Not in ABSL log/inernal/flags.h

// Set color messages logged to stderr (if supported by terminal).
ABSL_DECLARE_FLAG(bool, colorlogtostderr);

// Sets the maximum number of seconds which logs may be buffered for.
ABSL_DECLARE_FLAG(int, logbufsecs);

// If specified, logfiles are written into this directory instead of the
// default logging directory.
ABSL_DECLARE_FLAG(std::string, log_dir);

// Set the log file mode.
ABSL_DECLARE_FLAG(int, logfile_mode);

// Sets the path of the directory into which to put additional links
// to the log files.
ABSL_DECLARE_FLAG(std::string, log_link);

// Sets the maximum log file size (in MB).
ABSL_DECLARE_FLAG(int, max_log_size);

// Sets whether to avoid logging to the disk if the disk is full.
ABSL_DECLARE_FLAG(bool, stop_logging_if_full_disk);

#endif  // OR_TOOLS_BASE_FLAGS_H_
