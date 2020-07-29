#[=======================================================================[.rst:
FindProtobuf
--------

This module determines the Protobuf library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``protobuf::libprotobuf`` and
``protobuf::protoc``, if Protobuf has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Protobuf_FOUND          - True if Protobuf found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(PROTOBUF REQUIRED protobuf IMPORTED_TARGET GLOBAL)
add_library(protobuf::libprotobuf ALIAS PkgConfig::PROTOBUF)

find_program(PROTOC_EXEC protoc REQUIRED)
add_executable(protobuf::protoc IMPORTED GLOBAL)
set_target_properties(protobuf::protoc PROPERTIES
  IMPORTED_LOCATION ${PROTOC_EXEC}
)
