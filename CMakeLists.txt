cmake_minimum_required(VERSION 3.14)

# TODO: Figure out how to add profiling. Then profile!

project(
  swtor_combat_explorer
  VERSION 0.1.0
  DESCRIPTION "Glean insights from combat logs to up your game"
  HOMEPAGE_URL "https://example.com/"
  LANGUAGES CXX
)

include(cmake/prelude.cmake)
include(FetchContent)
include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com//jtv/libpqxx.git
  GIT_TAG 7.10.1
)

FetchContent_MakeAvailable(libpqxx)

FetchContent_Declare(
  gflags
  GIT_REPOSITORY https://github.com/gflags/gflags.git
  GIT_TAG v2.2.2
)

FetchContent_MakeAvailable(gflags)

find_package(
  Boost
  REQUIRED
  COMPONENTS log
)

# ---- Declare parser library ----

add_library(
  swtor_combat_explorer_lib
  STATIC
  source/lib.cpp
  source/timestamps.cpp
  source/log_parser.cpp
  source/log_parser_helpers.cpp
  source/logging.cpp
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

target_compile_options(
  swtor_combat_explorer_lib
  PRIVATE -pg
)

target_link_options(
  swtor_combat_explorer_lib
  PRIVATE -pg
)

# ---- Declare populator library ----

add_library(
  swtor_combat_populate_db_lib
  STATIC
  source/db_populator.cpp
)

target_include_directories(
  swtor_combat_populate_db_lib ${warning_guard}
  PUBLIC 
  "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
  SYSTEM "${libpqxx_BINARY_DIR}/include"
)

target_link_libraries(
  swtor_combat_populate_db_lib
  PRIVATE swtor_combat_explorer_lib
  Boost::log
  pqxx
  pq
)

target_compile_features(
  swtor_combat_populate_db_lib
  PUBLIC cxx_std_20
)

target_compile_options(
  swtor_combat_populate_db_lib
  PRIVATE -pg
)

target_link_options(
  swtor_combat_populate_db_lib
  PRIVATE -pg
)

# ---- Declare parser executable ----

# Currently the driver for the parser to just display parsed data.
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

target_compile_options(
  swtor_combat_explorer_exe
  PRIVATE -pg
)

target_link_options(
  swtor_combat_explorer_exe
  PRIVATE -pg
)

# ---- Declare populator executable ----

# This uses the parser to populate a database.
add_executable(
  swtor_combat_populate_db_exe
  source/swtor_combat_populate_db.cpp
)

set_property(
  TARGET swtor_combat_populate_db_exe
  PROPERTY OUTPUT_NAME swtor_combat_populate_db
)

target_include_directories(
  swtor_combat_populate_db_exe ${warning_guard}
  PUBLIC
  "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
  SYSTEM "${libpqxx_BINARY_DIR}/include"
)


target_compile_features(
  swtor_combat_populate_db_exe
  PRIVATE cxx_std_20
)

target_link_libraries(
  swtor_combat_populate_db_exe
  PRIVATE swtor_combat_populate_db_lib
  Boost::log
  pqxx
  pq
)

target_compile_options(
  swtor_combat_populate_db_exe
  PRIVATE -pg
)

target_link_options(
  swtor_combat_populate_db_exe
  PRIVATE -pg
)

# ---- Declare database searcher executable ----

# This uses pqxx to search the database
add_executable(
  swtor_combat_search_db_exe
  source/swtor_combat_search_db.cpp
)

set_property(
  TARGET swtor_combat_search_db_exe
  PROPERTY OUTPUT_NAME swtor_combat_search_db
)

target_include_directories(
  swtor_combat_search_db_exe ${warning_guard}
  PUBLIC
  "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
  SYSTEM "${libpqxx_BINARY_DIR}/include"
)


target_compile_features(
  swtor_combat_search_db_exe
  PRIVATE cxx_std_20
)

target_link_libraries(
  swtor_combat_search_db_exe
  Boost::log
  gflags
  pqxx
  pq
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

