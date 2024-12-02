set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS
  unit_test_framework REQUIRED
)
file(GLOB_RECURSE source_list
  "*.h"
  "*.cpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_executable(segregated_storage_test ${source_list})
target_link_libraries(segregated_storage_test
  PRIVATE
    segregated_storage
    Boost::unit_test_framework
)
add_test(segregated_storage_test segregated_storage_test)
