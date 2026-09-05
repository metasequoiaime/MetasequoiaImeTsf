set(TEST_EXECUTABLE_NAME "msimeui-tests")

add_executable(${TEST_EXECUTABLE_NAME}
    src/main.cpp
    src/test_layout.cpp
    includes/test_framework.h
)

# The test sources include their headers as "tests/includes/...", mirroring how
# server/ lays its tests out.
target_include_directories(${TEST_EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(${TEST_EXECUTABLE_NAME} PRIVATE msimeui)

if(MSVC)
    target_compile_options(${TEST_EXECUTABLE_NAME} PRIVATE /W4 /permissive-)
endif()

add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
