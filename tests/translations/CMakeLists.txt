cmake_minimum_required(VERSION 3.16)
project(TranslationTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test)

# Only Qt Core is required: the helpers are header-only and do not need a
# display server, SDL or the generated application headers.
add_executable(TranslationTests
    testtranslations.cpp
    ../../src/translations.h
)
target_include_directories(TranslationTests PRIVATE ../../src)
target_link_libraries(TranslationTests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME TranslationTests COMMAND TranslationTests -o -,txt -o translation-results.txt,txt)
set_tests_properties(TranslationTests PROPERTIES TIMEOUT 60)
