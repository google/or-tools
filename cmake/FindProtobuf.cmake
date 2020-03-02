#[=======================================================================[.rst:
FindProtobuf
--------

This module determines the Protobuf library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Protobuf::Protobuf``, if
Protobuf has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Protobuf_FOUND          - True if Protobuf found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(PROTOBUF REQUIRED protobuf IMPORTED_TARGET GLOBAL)
add_library(protobuf::libprotobuf ALIAS PkgConfig::PROTOBUF)
