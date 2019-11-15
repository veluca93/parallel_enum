cmake_minimum_required(VERSION 3.5)

project(parallel-enum)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/gflags)


find_package(MPI)

add_library(
  lib
  enumerable/clique.cpp
  enumerable/diam2kplex.cpp
  permute/permute.cpp
  util/cuckoo.cpp
  util/fastio.cpp
  util/graph.cpp
  util/binary_search.cpp
  util/dynarray.cpp
)


target_link_libraries(lib PUBLIC gflags::gflags absl::strings absl::memory absl::span)
target_include_directories(lib PUBLIC .)


add_executable(text_ui ui/text_ui.cpp)
add_executable(stats ui/stats.cpp)
target_link_libraries(text_ui lib)
target_link_libraries(stats lib)

if (MPI_CXX_FOUND)
  target_include_directories(text_ui SYSTEM PRIVATE ${MPI_CXX_INCLUDE_PATH})
  target_link_libraries(text_ui ${MPI_CXX_LIBRARIES})
  target_compile_definitions(text_ui PRIVATE PARALLELENUM_USE_MPI)
endif (MPI_CXX_FOUND)
