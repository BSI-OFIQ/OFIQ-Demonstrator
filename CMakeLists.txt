cmake_minimum_required(VERSION 3.26)

set(project_name "OFIQDemonstrator")

file(READ "Version.txt" ver)

string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${ver})
set(ver_major ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${ver})
set(ver_minor ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${ver})
set(ver_patch ${CMAKE_MATCH_1})

string(REGEX MATCH "VERSION_SUFFIX ([0-9a-zA-Z]*)" _ ${ver})
set(ver_suffix ${CMAKE_MATCH_1})

set(FULL_PROJECT_VERSION ${ver_major}.${ver_minor}.${ver_patch})
message("version: ${FULL_PROJECT_VERSION}")

add_compile_definitions(PROJECT_VERSION_MAJOR=${ver_major})
add_compile_definitions(PROJECT_VERSION_MINOR=${ver_minor})
add_compile_definitions(PROJECT_VERSION_PATCH=${ver_patch})
add_compile_definitions(PROJECT_VERSION_SUFFIX=${ver_suffix})
add_compile_definitions(PROJECT_VERSION=${FULL_PROJECT_VERSION})

SET(ARCHITECTURE x64 CACHE STRING "x64 or Win32 for Windows")
SET(OS windows CACHE STRING "Operating System windows, linux or mac")

# more `FetchContent_Declare`s (if any). They should all be declared
# before any calls to FetchContent_Make_available (see docs for why).
# more `FetchContent_MakeAvailable`s (if any).


if(UNIX AND NOT APPLE)
    project(${project_name} VERSION ${ver_major}.${ver_minor}.${ver_patch} LANGUAGES CXX)
    include(source/CMakeLists.ubuntu.cmake)
elseif(APPLE)
    project(${project_name} VERSION ${ver_major}.${ver_minor}.${ver_patch} LANGUAGES CXX)
    include(source/CMakeLists.macos.cmake)
else(APPLE)
    project(${project_name} VERSION ${ver_major}.${ver_minor}.${ver_patch} LANGUAGES CXX)
    include(source/CMakeLists.windows.cmake)
endif (UNIX AND NOT APPLE)

