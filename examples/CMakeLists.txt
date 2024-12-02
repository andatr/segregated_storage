file(GLOB_RECURSE source_list
  "*.h"
  "*.cpp"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_executable(segregated_storage_examples ${source_list})
target_link_libraries(segregated_storage_examples
  PRIVATE
    segregated_storage
)
