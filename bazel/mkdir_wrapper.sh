#!/usr/bin/env bash
# Copyright 2021 The Cross-Media Measurement Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Wrapper which creates the directory specified as $1, then executing the
# command in $2 with the remaining arguments.
#
# This is to work around https://github.com/bazelbuild/bazel/issues/6393

readonly directory="$1"
readonly command="$2"
shift 2

mkdir -p "${directory}"
exec "${command}" "$@"