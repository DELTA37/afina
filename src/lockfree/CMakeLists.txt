# build service
set(SOURCE_FILES
  thread_local.cpp
  applier.cpp
)

add_library(FlatCombine ${SOURCE_FILES})
target_link_libraries(FlatCombine ${CMAKE_THREAD_LIBS_INIT})
