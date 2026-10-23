cmake_minimum_required(VERSION 3.17)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ADD_DEFINITIONS(-DUNICODE)
# ADD_DEFINITIONS(-D_UNICODE)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/submodules/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

option(TF2BD_ENABLE_DISCORD_INTEGRATION "Enable discord integration" on)
option(TF2BD_ENABLE_TESTS "Enable test compilation" off)

include(cmake/init-preproject.cmake)
	project(tf2_bot_detector)
include(cmake/init-postproject.cmake)

# apparent bug in VS 16.9.1 makes error messages unclickable because of wrong paths
if (MSVC)
	add_compile_options(/FC)
endif()

set_warning_level(3)

# The vendored mh/ headers (formerly the mh_stuff submodule) are provided by tf2_bot_detector::common,
# which carries their include path, MH_* defines, fmt and C++20 as PUBLIC usage requirements.

add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)
add_subdirectory(submodules/glad2)
# add_subdirectory(submodules/imgui_desktop)

add_subdirectory(tf2_bot_detector_common)
add_subdirectory(tf2_bot_detector_renderer)
add_subdirectory(tf2_bot_detector)

