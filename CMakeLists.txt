cmake_minimum_required(VERSION 3.20)
project(DataCase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(datacase_storage
  src/storage/Pager.cpp
  src/storage/BufferPoolManager.cpp
)
target_include_directories(datacase_storage PUBLIC include)

add_library(datacase_index
  src/index/BTreeIndex.cpp
)
target_include_directories(datacase_index PUBLIC include)
target_link_libraries(datacase_index PUBLIC datacase_storage)

add_library(datacase_query
  src/query/QueryParser.cpp
)
target_include_directories(datacase_query PUBLIC include)

add_library(datacase_execution
  src/execution/ExecutionEngine.cpp
)
target_include_directories(datacase_execution PUBLIC include)
target_link_libraries(datacase_execution PUBLIC datacase_query datacase_index)

add_executable(datacase_cli src/main.cpp)
target_link_libraries(datacase_cli PRIVATE datacase_execution)

if (BUILD_TESTING)
  find_package(GTest CONFIG QUIET)
  if (GTest_FOUND)
    add_executable(datacase_tests
      tests/StorageBufferTests.cpp
      tests/BTreeIndexTests.cpp
      tests/QueryParserTests.cpp
    )
    target_link_libraries(datacase_tests PRIVATE datacase_storage datacase_index datacase_query datacase_execution GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(datacase_tests)
  else()
    message(WARNING \"GTest not found; datacase_tests target is skipped\") 
  endif()
endif()
