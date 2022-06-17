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

#ifndef OR_TOOLS_BASE_PATH_H_
#define OR_TOOLS_BASE_PATH_H_

#include <initializer_list>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"

// A set of file pathname manipulation routines.
// Calls to each of the following functions assume their input is
// well-formed (for some currently nebulous definition of the word).
// You can get a well-formed path by using file::CleanPath(), and
// future versions of this API may require this.
//
// This collection is largely modelled on Python's os.path module.

// Filenames are restricted to ASCII characters in most filesystems at
// Google.  There are additional restrictions and quirks for some
// filesystems.  For example, see file/colossus/base/cfs_path.h for
// filename character set restrictions for Colossus.
//
// One additional filename quirk: for legacy reasons, filenames of
// the forms:
//  localhost:pathname
//  hostname:pathname
//  hostname.domainname:pathname
// will all route to //file/localfile and use only the pathname part, so
// long as hostname/hostname.domainname refer to the current host.  The
// primary implication of this behavior is that filenames that do not
// begin with a leading '/' and that have a ':' before any '/' may be
// interpreted differently depending on the hostname where they are
// evaluated.

namespace file {

namespace internal {
// Not part of the public API.
std::string JoinPathImpl(bool honor_abs,
                         std::initializer_list<absl::string_view> paths);
}  // namespace internal

// Join multiple paths together.
// JoinPath and JoinPathRespectAbsolute have slightly different semantics.
// JoinPath unconditionally joins all paths together, whereas
// JoinPathRespectAbsolute ignores any segments prior to the last absolute
// path.  For example:
//
//  Arguments                  | JoinPath            | JoinPathRespectAbsolute
//  ---------------------------+---------------------+-----------------------
//  'foo',   'bar'             | foo/bar             | foo/bar
//  '/foo',  'bar'             | /foo/bar            | /foo/bar
//  '/foo/', 'bar'             | /foo/bar            | /foo/bar
//  '/foo',  '/bar'            | /foo/bar            | /bar
//  '/foo/', '/bar'            | /foo/bar            | /bar
//  '/foo',  '/bar', '/baz'    | /foo/bar/baz        | /baz
//
// The first path may be relative or absolute.  All subsequent paths will be
// treated as relative paths, regardless of whether or not they start with a
// leading '/'.  That is, all paths will be concatenated together, with the
// appropriate path separator inserted in between.
// Arguments must be convertible to absl::string_view.
//
// Usage:
// string path = file::JoinPath("/cns", dirname, filename);
// string path = file::JoinPath(FLAGS_test_srcdir, filename);
//
// 0, 1, 2-path specializations exist to optimize common cases.
inline std::string JoinPath() { return std::string(); }
inline std::string JoinPath(absl::string_view path) {
  return std::string(path.data(), path.size());
}
std::string JoinPath(absl::string_view path1, absl::string_view path2);
template <typename... T>
inline std::string JoinPath(absl::string_view path1, absl::string_view path2,
                            absl::string_view path3, const T&... args) {
  return internal::JoinPathImpl(/*honor_abs=*/false,
                                {path1, path2, path3, args...});
}

// Join multiple paths together, respecting intermediate absolute paths.
// All paths will be joined together, but if any of the paths is absolute
// (as defined by IsAbsolutePath()), all prior path segments will be ignored.
// This is the behavior of the old File::JoinPath().
// Arguments must be convertible to absl::string_view.
//
// Usage:
// string path = file::JoinPathRespectAbsolute("/cns", dirname, filename);
// string path = file::JoinPathRespectAbsolute(FLAGS_test_srcdir, filename);
template <typename... T>
inline std::string JoinPathRespectAbsolute(const T&... args) {
  return internal::JoinPathImpl(/*honor_abs=*/true, {args...});
}

// Return true if path is absolute.
bool IsAbsolutePath(absl::string_view path);

// If path is non-empty and doesn't already end with a slash, append one
// to the end.
std::string AddSlash(absl::string_view path);

// Returns the part of the path before the final "/", EXCEPT:
// * If there is a single leading "/" in the path, the result will be the
//   leading "/".
// * If there is no "/" in the path, the result is the empty prefix of the
//   input string.
absl::string_view Dirname(absl::string_view path);

// Return the parts of the path, split on the final "/".  If there is no
// "/" in the path, the first part of the output is empty and the second
// is the input. If the only "/" in the path is the first character, it is
// the first part of the output.
std::pair<absl::string_view, absl::string_view> SplitPath(
    absl::string_view path);

// Returns the part of the path after the final "/".  If there is no
// "/" in the path, the result is the same as the input.
// Note that this function's behavior differs from the Unix basename
// command if path ends with "/". For such paths, this function returns the
// empty string.
absl::string_view Basename(absl::string_view path);

// Returns the part of the basename of path prior to the final ".".  If
// there is no "." in the basename, this is equivalent to file::Basename(path).
absl::string_view Stem(absl::string_view path);

// Returns the part of the basename of path after the final ".".  If
// there is no "." in the basename, the result is empty.
absl::string_view Extension(absl::string_view path);

// ********************************************************
// Path cleaning utilities.
//
// NOTE: FileFactory implementations should probably clean paths they
// receive as needed.  They should only clean the pieces of path they
// understand (in the case of wrapping-functionality like
// /readaheadfile or /encryptedfile).  It cannot universally be done
// cleanly with common logic.
//
// Common sorts of surprises along those lines:
//  * file/zipfile allows '/' and '.' in file *names* (and has no directory
//    structure).
//  * Colossus probably shouldn't collapse "ttl=8/../" into "/"

// Collapse duplicate "/"s, resolve ".." and "." path elements, remove
// trailing "/".
//
// NOTE: This respects relative vs. absolute paths, but does not
// invoke any system calls (getcwd(2)) in order to resolve relative
// paths wrt actual working directory.  That is, this is purely a
// string manipulation, completely independent of process state.
std::string CleanPath(absl::string_view path);

// Similar to CleanPath, but only collapses duplicate "/"s.  (Some
// filesystems allow "." and ".." in filenames.)
std::string CollapseSlashes(absl::string_view path);

}  // namespace file

#endif  // OR_TOOLS_BASE_PATH_H_
