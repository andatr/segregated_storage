cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
enable_testing()

project(object_pool VERSION 0.0.1 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

if(MSVC)
  add_compile_options(/wd4714)   # marked as __forceinline not inlined
  add_compile_options(/wd4503)   # decorated name length exceeded, name was truncated
  add_compile_options(/wd26812)  # the enum type type-name is unscoped. Prefer enum class over enum
  add_link_options(/ignore:4099) # missing pdb files
  add_link_options(/verbose:lib)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
else(MSVC)
  add_compile_options(-Wall -Wextra -pedantic)
endif(MSVC)

find_package(Boost COMPONENTS
  unit_test_framework
REQUIRED)

file(GLOB_RECURSE source_list
  "include/*.h"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_library(object_pool INTERFACE ${source_list})
target_include_directories(object_pool
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

install(TARGETS object_pool INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_subdirectory(examples)
add_subdirectory(test)
