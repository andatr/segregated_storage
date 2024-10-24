file(GLOB_RECURSE source_list
  "include/*.h"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list})
add_library(object_pool INTERFACE ${source_list})
target_include_directories(object_pool
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)