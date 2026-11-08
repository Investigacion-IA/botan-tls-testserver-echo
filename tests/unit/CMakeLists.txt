# Prefer the distribution's GoogleTest (libgtest-dev, libgmock-dev) over stray copies in /usr/local.
find_package(GTest CONFIG QUIET
             HINTS "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/GTest"
             NO_DEFAULT_PATH)
if(NOT GTest_FOUND)
   find_package(GTest REQUIRED)
endif()

include(GoogleTest)

add_executable(${PROJECT_NAME}_tests
               common/NetErrorsTest.cpp
               echo/EchoSessionTest.cpp
               server/CommandLineTest.cpp
               server/TcpListenerTest.cpp
               server/TlsSessionHandlerTest.cpp
               server/TlsEchoIntegrationTest.cpp
               tls/ServerPolicyTest.cpp
               tls/TlsPolicyLoaderTest.cpp)

target_compile_definitions(${PROJECT_NAME}_tests
                           PRIVATE
                             TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}_tests
                      PRIVATE
                        ${PROJECT_NAME}_core
                        GTest::gmock
                        GTest::gtest_main)

gtest_discover_tests(${PROJECT_NAME}_tests)
