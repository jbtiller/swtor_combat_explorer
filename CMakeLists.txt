cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
  swtor_combat_explorer
  VERSION 0.1.0
  DESCRIPTION "Glean insights from combat logs to up your game"
  HOMEPAGE_URL "https://example.com/"
  LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

find_package(
  Boost
  REQUIRED
  COMPONENTS log
)

# ---- Declare library ----

add_library(
  swtor_combat_explorer_lib
  STATIC
  source/lib.cpp
  source/timestamps.cpp
  source/log_parser.cpp
  source/log_parser_helpers.cpp
)

target_include_directories(
  swtor_combat_explorer_lib ${warning_guard}
  PUBLIC
  "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_link_libraries(
  swtor_combat_explorer_lib
   Boost::log
)

target_compile_features(
  swtor_combat_explorer_lib
  PUBLIC cxx_std_20
)

# ---- Declare executable ----

add_executable(
  swtor_combat_explorer_exe
  source/main.cpp
)
add_executable(
  swtor_combat_explorer::exe
  ALIAS swtor_combat_explorer_exe
)

set_property(
  TARGET swtor_combat_explorer_exe
  PROPERTY OUTPUT_NAME swtor_combat_explorer
)

target_compile_features(
  swtor_combat_explorer_exe
  PRIVATE cxx_std_20
)

target_link_libraries(
  swtor_combat_explorer_exe
  PRIVATE swtor_combat_explorer_lib
  Boost::log
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT swtor_combat_explorer_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of swtor_combat_explorer"
  )
endif()

include(cmake/dev-mode.cmake)

