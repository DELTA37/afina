# build service
set(SOURCE_FILES
  SimpleTest.cpp 
)

add_executable(runThreadLocalTests ${SOURCE_FILES} ${BACKWARD_ENABLE})
target_link_libraries(runThreadLocalTests FlatCombine gtest gmock gmock_main pthread)

add_backward(runThreadLocalTests)
add_test(runThreadLocalTests runThreadLocalTests)
