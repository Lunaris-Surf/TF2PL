cmake_policy(SET CMP0091 NEW) # Enable [CMAKE_]MSVC_RUNTIME_LIBRARY

# VERSION is the single source of truth for release versions. Keep it to three
# numeric components (for example: 2.0.0). CI appends its run number as the
# fourth component without requiring workflow changes.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" TF2PL_VERSION_FILE)
string(STRIP "${TF2PL_VERSION_FILE}" TF2PL_VERSION_FILE)
if (NOT TF2PL_VERSION_FILE MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
	message(FATAL_ERROR "VERSION must contain a semantic version such as 2.0.0 (found: '${TF2PL_VERSION_FILE}')")
endif()

set(TF2BD_VERSION_MAJOR "${CMAKE_MATCH_1}" CACHE STRING "TF2PL major version." FORCE)
set(TF2BD_VERSION_MINOR "${CMAKE_MATCH_2}" CACHE STRING "TF2PL minor version." FORCE)
set(TF2BD_VERSION_PATCH "${CMAKE_MATCH_3}" CACHE STRING "TF2PL patch version." FORCE)
