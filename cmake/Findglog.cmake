# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#[=======================================================================[.rst:
Findglog
--------

This module determines the glog library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``glog::glog``, if
glog has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

glog_FOUND          - True if glog found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLOG REQUIRED libglog IMPORTED_TARGET GLOBAL)
add_library(glog::glog ALIAS PkgConfig::GLOG)
