cmake_minimum_required(VERSION 3.16)
project(CliArgumentsTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test)

# Header only helpers, so no display server, SDL or generated headers are needed.
add_executable(CliArgumentsTests
    testcliarguments.cpp
    ../../src/cliarguments.h
)
target_include_directories(CliArgumentsTests PRIVATE ../../src)
target_link_libraries(CliArgumentsTests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME CliArgumentsTests COMMAND CliArgumentsTests -o -,txt -o cli-arguments-results.txt,txt)
set_tests_properties(CliArgumentsTests PROPERTIES TIMEOUT 60)
