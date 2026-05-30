cmake_minimum_required(VERSION 3.20)
project(C2-Raptor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-g3 -O0 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")

add_custom_target(setup
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup.sh
    COMMENT "Installing system dependencies"
)

add_library(net SHARED IMPORTED)
set_target_properties(net PROPERTIES
    IMPORTED_LOCATION             ${CMAKE_CURRENT_SOURCE_DIR}/../libs/net/libnet.so
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../libs/net/include
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 sqlite3)

if(NOT SQLITE3_FOUND)
  message(FATAL_ERROR
    "\n[ERROR] sqlite3 not found!"
    "\n  Run:  cmake --build build/debug --target setup"
    "\n  Or manually:"
    "\n    Ubuntu/Debian: sudo apt install libsqlite3-dev"
    "\n    Arch:          sudo pacman -S sqlite"
    "\n    Fedora:        sudo dnf install sqlite-devel\n")
endif()

message(STATUS "Found sqlite3: ${SQLITE3_VERSION} — ${SQLITE3_LINK_LIBRARIES}")

find_package(Drogon QUIET)

if(NOT Drogon_FOUND)
  message(FATAL_ERROR
    "\n[ERROR] Drogon not found!"
    "\n  Run:  cmake --build build/debug --target setup\n")
endif()

message(STATUS "Found Drogon: ${Drogon_VERSION}")

file(GLOB_RECURSE CORE_SOURCES   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../common/*.cpp")
list(FILTER CORE_SOURCES   EXCLUDE REGEX ".*/build/.*")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/build/.*")

add_executable(raptor ${CORE_SOURCES} ${COMMON_SOURCES})

target_include_directories(raptor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/net/include
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(raptor PRIVATE
    net
    ${SQLITE3_LIBRARIES}
    Drogon::Drogon
    OpenSSL::Crypto # !
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_link_options(raptor PRIVATE -fsanitize=address,undefined)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_link_options(raptor PRIVATE -static-libgcc -static-libstdc++)
endif()

message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "Include dirs: ${CMAKE_CURRENT_SOURCE_DIR}/..")
