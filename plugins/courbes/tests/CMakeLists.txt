# Tests de l'evolution de Droz Courbes (hors CI : longs, ~2 min). A lancer avant chaque version :
#   cmake -S plugins/courbes/tests -B build-tests -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-tests --target evolution && ./build-tests/evolution
cmake_minimum_required(VERSION 3.25)
project(TestsCourbes LANGUAGES C CXX)
set(DROZ_PLUGINS courbes CACHE STRING "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. droz)
add_executable(evolution evolution.cpp)
target_link_libraries(evolution PRIVATE DrozCourbes droz::dsp)
target_compile_definitions(evolution PRIVATE $<TARGET_PROPERTY:DrozCourbes,COMPILE_DEFINITIONS>)
target_include_directories(evolution PRIVATE $<TARGET_PROPERTY:DrozCourbes,INCLUDE_DIRECTORIES>)
