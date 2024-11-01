# Copyright 2010-2024 Google LLC
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
FindMosek
--------

This module determines the Mosek library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``mosek::mosek``, if
Mosek has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

MOSEK_FOUND          - True if Mosek found.

Hints
^^^^^

A user may set ``MOSEK_PLATFORM_DIR`` to a Mosek installation platform
directoru to tell this module where to look, or ``MOSEK_BASE`` to point 
to path where the ``mosek/`` directory is located.
#]=======================================================================]
set(MOSEK_FOUND FALSE)
message(STATUS "Locating MOSEK")

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindMosek only works if either C or CXX language is enabled")
endif()

if(APPLE) 
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        SET(MOSEK_PLATFORM_NAME "osxaarch64")
    elseif()
        message(FATAL_ERROR "Mosek not supported for ${CMAKE_SYSTEM} / ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        SET(MOSEK_PLATFORM_NAME "linuxaarch64")
    else()
        SET(MOSEK_PLATFORM_NAME "linux64x86")
    endif()
elseif(MSVC)
    SET(MOSEK_PLATFORM_NAME "win64x86")
else()
    message(FATAL_ERROR "Mosek not supported for ${CMAKE_SYSTEM}")
endif()

function(FindMosekPlatformInPath RESULT PATH)
    SET(${RESULT} "")
    if(EXISTS "${PATH}/mosek")
        SET(dirlist "")
        FILE(GLOB entries LIST_DIRECTORIES true "${PATH}/mosek/*")
        FOREACH(f ${entries})
            if(IS_DIRECTORY "${f}")
                get_filename_component(bn "${f}" NAME)
                if("${bn}" MATCHES "^[0-9]+[.][0-9]+$") 
                    if (${bn} GREATER_EQUAL "10.0")
                        LIST(APPEND dirlist "${bn}")
                    endif()
                endif()
            endif()
        ENDFOREACH()
        LIST(SORT dirlist COMPARE NATURAL ORDER DESCENDING)

        if(MOSEK_PLATFORM_NAME) 
            foreach(MOSEK_VERSION ${dirlist})
                SET(MSKPFDIR "${PATH}/mosek/${MOSEK_VERSION}/tools/platform/${MOSEK_PLATFORM_NAME}")
                if(EXISTS "${MSKPFDIR}")
                    SET(${RESULT} ${MSKPFDIR} PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endif()
    endif()
endfunction()


function(MosekVersionFromPath RESVMAJOR RESVMINOR PATH)
    execute_process(COMMAND "${PATH}/bin/mosek" "-v" RESULT_VARIABLE CMDRES OUTPUT_VARIABLE VERTEXT)
    if (${CMDRES} EQUAL 0)
        if (${VERTEXT} MATCHES "^MOSEK version [0-9]+[.][0-9]+[.][0-9]+")
            string(REGEX REPLACE "^MOSEK version ([0-9]+)[.]([0-9]+).*" "\\1;\\2" MSKVER ${VERTEXT})

            list(GET MSKVER 0 VMAJOR)
            list(GET MSKVER 1 VMINOR)
            set(${RESVMAJOR} "${VMAJOR}" PARENT_SCOPE)
            set(${RESVMINOR} "${VMINOR}" PARENT_SCOPE)

            #message(STATUS "mskver = ${VMAJOR}.${VMINOR} -> ${${RESVMAJOR}}.${${RESVMINOR}}")
            #message(STATUS "    ${RESVMAJOR} = ${${RESVMAJOR}}")
            #message(STATUS "    ${RESVMINOR} = ${${RESVMINOR}}")
            return()
        endif()
    endif()
endfunction()


# Where to look for MOSEK:
# Standard procedure in Linux/OSX is to install MOSEK in the home directory, i.e.
#   $HOME/mosek/X.Y/...
# Option 1. The user can specify when running CMake where the MOSEK platform directory is located, e.g.
#     -DMOSEK_PLATFORM_DIR=$HOME/mosek/10.2/tools/platform/linux64x86/ 
#   in which case no search is performed.
# Option 2. The user can specify MOSEK_ROOT when running cmake. MOSEK_ROOT is
#   the directory where the root mosek/ tree is located.
# Option 3. Automatic search. We will then attempt to search in the default
#   locations, and if that fails, assume it is installed in a system location.
# For option 2 and 3, the newest MOSEK version will be chosen if more are available.

if(MOSEK_PLATFORM_DIR)
    # User defined platform dir directly
elseif(MOSEK_BASE)
    # Look under for MOSEK_ROOT/X.Y
    FindMosekPlatformInPath(MOSEK_PLATFORM_DIR "${MOSEK_BASE}")
    if(NOT MOSEK_PLATFORM_DIR)
        message(FATAL_ERROR "  Could not locate MOSEK platform directory under ${MOSEK_BASE}")
    endif()
endif()
if(NOT MOSEK_PLATFORM_DIR)
    # Look in users home dir
    if(EXISTS $ENV{HOME})
        FindMosekPlatformInPath(MOSEK_PLATFORM_DIR "$ENV{HOME}")
    endif()
endif()
if(NOT MOSEK_PLATFORM_DIR)
    # Look in users home dir
    if(EXISTS "$ENV{HOMEDRIVE}$ENV{HOMEPATH}")
        FindMosekPlatformInPath(MOSEK_PLATFORM_DIR "$ENV{HOMEDRIVE}$ENV{HOMEPATH}")
    endif()
endif()

if(NOT MOSEK_PLATFORM_DIR)
  message(FATAL_ERROR "  MOSEK_PLATFORM_DIR could not be detected")
else()
  MosekVersionFromPath(MSKVMAJOR MSKVMINOR "${MOSEK_PLATFORM_DIR}")
  if(MSKVMAJOR)
    message(STATUS "  MOSEK ${MSKVMAJOR}.${MSKVMINOR}: ${MOSEK_PLATFORM_DIR}")
    set(MOSEK_PFDIR_FOUND TRUE)
  else()
    message(STATUS "  Found MOSEK_PLATFORM_DIR, but failed to determine version")
  endif()
endif()

if(MOSEK_PFDIR_FOUND AND MSKVMAJOR AND MSKVMINOR AND NOT TARGET mosek::mosek)
  add_library(mosek::mosek UNKNOWN IMPORTED)
  
  find_path(MOSEKINC mosek.h HINTS ${MOSEK_PLATFORM_DIR}/h)
  if(WIN32)
      find_library(LIBMOSEK mosek64_ HINTS ${MOSEK_PLATFORM_DIR}/bin)
  else()
      find_library(LIBMOSEK mosek64 HINTS ${MOSEK_PLATFORM_DIR}/bin)
  endif()

  if(LIBMOSEK) 
      set_target_properties(mosek::mosek PROPERTIES IMPORTED_LOCATION ${LIBMOSEK})
  endif()

  if (MOSEKINC)
      target_include_directories(mosek::mosek INTERFACE "${MOSEKINC}")
  endif()
elseif(NOT TARGET mosek::mosek)
  add_library(mosek::mosek UNKNOWN IMPORTED)
  
  find_path(MOSEKINC mosek.h)
  find_library(LIBMOSEK mosek64)

  if(LIBMOSEK) 
      set_target_properties(mosek::mosek PROPERTIES IMPORTED_LOCATION ${LIBMOSEK})
  endif()

  if (MOSEKINC)
    target_include_directories(mosek::mosek INTERFACE "${MOSEKINC}")
  endif()
endif()
