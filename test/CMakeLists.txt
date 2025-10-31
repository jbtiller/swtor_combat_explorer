# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(swtor_combat_explorerTests LANGUAGES CXX)

include(FetchContent)

enable_testing()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(googletest)
message("${googletest_SOURCE_DIR}")

include(GoogleTest)

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com//jtv/libpqxx.git
  GIT_TAG 7.10.1
)

FetchContent_MakeAvailable(libpqxx)

find_package(
    Boost
    REQUIRED
    COMPONENTS log
)

# ---- Parser Tests ----

add_executable(
  swtor_combat_explorer_test
  source/swtor_combat_explorer_test.cpp
)

target_include_directories(
  swtor_combat_explorer_test
  PRIVATE SYSTEM ${googletest_SOURCE_DIR}/googletest/include/gtest
)  

target_link_libraries(
  swtor_combat_explorer_test
  PRIVATE swtor_combat_explorer_lib
  PRIVATE Boost::log
  GTest::gtest_main
)

target_compile_features(
  swtor_combat_explorer_test
  PRIVATE cxx_std_20
)

add_test(
  NAME swtor_combat_explorer_test
  COMMAND swtor_combat_explorer_test
)

# ---- Populator Tests ----

add_executable(
  swtor_combat_populate_db_test
  source/swtor_combat_populate_db_test.cpp
)

target_include_directories(
  swtor_combat_populate_db_test
  PRIVATE SYSTEM ${googletest_SOURCE_DIR}/googletest/include/gtest
  PRIVATE SYSTEM "${libpqxx_BINARY_DIR}/include"
)  

target_link_libraries(
  swtor_combat_populate_db_test
  PRIVATE swtor_combat_populate_db_lib
  PRIVATE Boost::log
  GTest::gtest_main
  pqxx
  pq
)

target_compile_features(
  swtor_combat_populate_db_test
  PRIVATE cxx_std_20
)

add_test(
  NAME swtor_combat_populate_db_test
  COMMAND swtor_combat_populate_db_test
)

# ---- End-of-file commands ----
