cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
cmake_policy(SET CMP0167 NEW)
enable_testing()

project(segregated_storage VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SEGREGATED_STORAGE_TEST "Enable tests (default ON)" ON)
option(SEGREGATED_STORAGE_EXAMPLES "Enable examples (default ON)" ON)

set(CMAKE_CXX_STANDARD 20)
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

file(GLOB_RECURSE source_list
  "include/*.h"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_library(segregated_storage INTERFACE ${source_list})
target_include_directories(segregated_storage
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  TARGETS segregated_storage
  EXPORT segregated_storageConfig
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  EXPORT segregated_storageConfig
  DESTINATION "share/cmake/segregated_storage"
)
write_basic_package_version_file(segregated_storageConfigVersion.cmake COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/segregated_storageConfigVersion.cmake"
  DESTINATION "share/cmake/segregated_storage"
)

if (SEGREGATED_STORAGE_EXAMPLES)
  add_subdirectory(examples)
endif()

if (SEGREGATED_STORAGE_TEST)
  add_subdirectory(test)
endif()


