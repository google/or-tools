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

pybind11_add_module(pywrap_model_builder_helper MODULE pywrap_model_builder_helper.cc)

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywrap_model_builder_helper PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
  set_property(TARGET pywrap_model_builder_helper APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(pywrap_model_builder_helper PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()

target_link_libraries(pywrap_model_builder_helper PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::pywrap_model_builder_helper ALIAS pywrap_model_builder_helper)
