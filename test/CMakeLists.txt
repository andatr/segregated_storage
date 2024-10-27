file(GLOB_RECURSE source_list
  "*.h"
  "*.cpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_executable(object_pool_test ${source_list})
target_link_libraries(object_pool_test
  PRIVATE
    object_pool
    Boost::unit_test_framework
)
add_test(object_pool_test object_pool_test)
