# build service
set(SOURCE_FILES
    flatcombine.cpp
)

add_library(FlatCombine ${SOURCE_FILES})
target_link_libraries(FlatCombine ${CMAKE_THREAD_LIBS_INIT})
